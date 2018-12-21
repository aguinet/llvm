#include "llvm/Transforms/Scalar/ScalarizeStack.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"

#define DEBUG_TYPE "scalarize-stack"

using namespace llvm;


namespace {

bool clobberRegister(LoadInst* Reg)
{
  GlobalVariable* GV = cast<GlobalVariable>(Reg->getPointerOperand());
  Function* F = Reg->getParent()->getParent();
  SmallVector<StoreInst*, 1> Stores; 
  for (auto* U: GV->users()) {
    if (auto* SI = dyn_cast<StoreInst>(U)) {
      if (SI->getParent()->getParent() == F) {
        Stores.push_back(SI);
      }
    }
  }
  for (auto* SI: Stores) {
    SI->eraseFromParent();
  }
  return true;
}

bool processFunc(Function& F, LoadInst* SPRegLoad, GlobalVariable* GVStack)
{
  LLVM_DEBUG(dbgs() << "in function '" << F.getName() << "', stack pointer loaded at " << *SPRegLoad << "\n");
  auto& Ctx = F.getContext();

  // Clobber the stack register
  clobberRegister(SPRegLoad);

  // TODO: check the result of this loading is an integer the size of a pointer!

  // TODO: some range analysis to figure out the stack size
  const size_t StackSize = 65536;
  // Allocate of stack of StackSize
  IRBuilder<> IRB(SPRegLoad);
  ArrayType* StackTy = ArrayType::get(Type::getInt8Ty(Ctx), StackSize);
  AllocaInst* Stack = IRB.CreateAlloca(StackTy, 0, nullptr, "stack");
  Type* I32Ty = Type::getInt32Ty(Ctx);
  Value* GStackPtr = IRB.CreateLoad(GVStack, "gstack_ptr");
  Value* I0 = ConstantInt::get(I32Ty, 0);
  IRB.CreateMemCpy(IRB.CreateInBoundsGEP(Stack, {I0, I0}), 1, GStackPtr, 1, StackSize);

  Value* StackPtr = IRB.CreateInBoundsGEP(Stack, {I0, ConstantInt::get(I32Ty,StackSize/2)}, "stackPtr");

  // We go throught the users of SPRegLoad, and replace add/sub by a GEP on
  // stack + ptrtoint. Save this list of ptrtoints.
  // We then take this list of ptrtoint, and replace chains of
  // ptrtoint/ops/inttoptr by a chain of GEPs
  bool Ret = false;
  SmallVector<User*, 8> TmpUsers(SPRegLoad->user_begin(), SPRegLoad->user_end());
  SmallVector<PtrToIntInst*, 8> PtrToInts;
  for (auto* U: TmpUsers) { 
    if (auto* BO = dyn_cast<BinaryOperator>(U)) {
      IRBuilder<> IRB(BO);
      Value* Cnt = (BO->getOperand(0) == SPRegLoad) ? BO->getOperand(1):BO->getOperand(0);
      switch (BO->getOpcode()) {
        case Instruction::Add:
          break;
        case Instruction::Sub:
          Cnt = IRB.CreateNeg(Cnt);
          break;
        default:
          LLVM_DEBUG(dbgs() << "unsupported operation: " << *BO << "\n");
          continue;
      };
      auto* GEP = IRB.CreateInBoundsGEP(StackPtr, {Cnt});
      auto* ToInt = new PtrToIntInst(GEP, BO->getType(), "", BO);
      BO->replaceAllUsesWith(ToInt);
      BO->eraseFromParent();
      PtrToInts.push_back(ToInt);
      //LLVM_DEBUG(dbgs() << "replace\n  " << *U << "\nwith\n  " << *ToInt << "\n");
      Ret = true;
    }
    else
    if (auto* I = dyn_cast<IntToPtrInst>(U)) {
      IRBuilder<> IRB(I);
      auto* BitCast = IRB.CreateBitCast(StackPtr, I->getType());
      I->replaceAllUsesWith(BitCast);
      I->eraseFromParent();
      //LLVM_DEBUG(dbgs() << "replace\n  " << *U << "\nwith\n  " << *StackPtr << "\n");
    }
    else {
      LLVM_DEBUG(dbgs() << "unsupported users of load(@GEP): " << *U << "\n");
      // TODO
    }
  }

  for (auto* PTI: PtrToInts) {
    auto* OrgPtr = PTI->getPointerOperand();
    TmpUsers.clear();
    std::copy(PTI->user_begin(), PTI->user_end(), std::back_inserter(TmpUsers));
    for (auto* U: TmpUsers) {
      auto* ITP = dyn_cast<IntToPtrInst>(U);
      if (!ITP) {
        LLVM_DEBUG(dbgs() << "unsupported user of PTI: " << *U << "\n");
        continue;
      }
      auto* BC = new BitCastInst(OrgPtr, ITP->getType(), "", ITP);
      ITP->replaceAllUsesWith(BC);
      ITP->eraseFromParent();
    }
    if (PTI->user_empty()) {
      PTI->eraseFromParent();
    }
  }

  if (SPRegLoad->user_empty()) {
    SPRegLoad->eraseFromParent();
  }
  return Ret;
}

}

namespace llvm {

bool ScalarizeStackPass::runImpl(Module& M) {
  errs() << "coucou\n";

  // TODO: get this from an external definition
  const char* SPRegName = "ESP";
  GlobalVariable* SPReg = M.getGlobalVariable(SPRegName);
  if (SPReg == nullptr) {
    LLVM_DEBUG(dbgs() << "unable to find the SP register '" << SPRegName << "'!\n");
    return false;
  }

  // Gather the users. We should have one per function.
  DenseMap<Function*, LoadInst*> Funcs;
  for (auto* U: SPReg->users()) {
    if (!isa<LoadInst>(U)) {
      LLVM_DEBUG(dbgs() << "unable to handle this instruction: " << *U << ", skipping it\n");
      continue;
    }
    auto* LI = cast<LoadInst>(U);
    Function* F = LI->getParent()->getParent();
    if (!Funcs.try_emplace(F, LI).second) {
      LLVM_DEBUG(dbgs() << "multiple users of SP reg in function '" << F->getName() << "', ignoring this module.!\n");
      return false;
    }
  }

  if (Funcs.size() == 0) {
    return false;
  }

  auto& Ctx = M.getContext();
  GlobalVariable* GVStack = new GlobalVariable(M, Type::getInt8PtrTy(Ctx), false, GlobalVariable::ExternalLinkage, nullptr, "g_stack");

  for (auto& FLI: Funcs) {
    processFunc(*FLI.first, FLI.second, GVStack);
  }

  // Run SROA, InstCombine
  return true;
}

PreservedAnalyses ScalarizeStackPass::run(Module& M, ModuleAnalysisManager &) {
  if (!runImpl(M))
    return PreservedAnalyses::all();

  return PreservedAnalyses::none();
}

} // llvm

// Legacy pass registration

namespace {

struct ScalarizeStackLegacyPass: public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  ScalarizeStackLegacyPass():
    ModulePass(ID)
  {
    initializeScalarizeStackLegacyPassPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module& M) override {
    return Impl.runImpl(M);
  }

private:
  ScalarizeStackPass Impl;
};

} // anonymous 

char ScalarizeStackLegacyPass::ID = 0;
INITIALIZE_PASS(ScalarizeStackLegacyPass, "scalarize-stack",
  "Scalarize stack (Miasm specific)", false, false)

ModulePass* llvm::createScalarizeStackPass() { return new ScalarizeStackLegacyPass{}; }

