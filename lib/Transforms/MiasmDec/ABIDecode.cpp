#include "llvm/Transforms/MiasmDec.h"
#include "llvm/Transforms/MiasmDec/ABIDecode.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "Stack.h"
#include "Tools.h"

#define DEBUG_TYPE "abi-decode"

using namespace llvm;

namespace {

Value* getGEPStackScalar(GetElementPtrInst* GEP)
{
  // Returns the first load or bitcast+load!
  for (auto* GU: GEP->users()) {
    if (auto* LI = dyn_cast<LoadInst>(GU)) {
      return LI;
    }
    else
    if (auto* BC = dyn_cast<BitCastInst>(GU)) {
      for (auto* BCU: BC->users()) {
        if (auto* LI = dyn_cast<LoadInst>(BCU)) {
          return BCU;
        }
      }
    }
  }
  return nullptr;
}

Instruction* getGEPStackScalarFinalPtr(GetElementPtrInst* GEP)
{
  for (auto* GU: GEP->users()) {
    if (auto* LI = dyn_cast<LoadInst>(GU)) {
      return GEP;
    }
    else
    if (auto* LI = dyn_cast<StoreInst>(GU)) {
      return GEP;
    }
    else
    if (auto* BC = dyn_cast<BitCastInst>(GU)) {
      return BC;
    }
  }
  return nullptr;
}

bool promoteStackToArgs(Function* F, LoadInst* SP)
{
  std::map<unsigned, SmallVector<GetElementPtrInst*, 1>> Args;
  SmallVector<Instruction*, 8> ToRemove;
  for (auto* U: SP->users()) {
    if (isa<CallInst>(U)) {
      // TODO: check this is memcpy
      // Remove this, useless now!
      ToRemove.push_back(cast<Instruction>(U));
      continue;
    }
    auto* GEP = dyn_cast<GetElementPtrInst>(U);
    if (!GEP) {
      LLVM_DEBUG(dbgs() << *U << "\n");
      llvm::report_fatal_error("unexpected user of SP");
    }
    if (GEP->getNumIndices() != 1) {
      LLVM_DEBUG(dbgs() << *U << "\n");
      llvm::report_fatal_error("unexpected number of indices in a GEP(SP)");
    }
    Value* Idx = *GEP->idx_begin();
    if (auto* C = dyn_cast<ConstantInt>(Idx)) {
      unsigned IdxV = C->getZExtValue();
      // TODO: 32768 comes from nowhere!
      if (IdxV >= 32768) {
        Args[IdxV-32768].push_back(GEP);
      }
    }
    else {
      LLVM_DEBUG(dbgs() << *U << ": unhandled GEP indice type in this GEP(SP). Ignored.\n");
    }
  }

  for (Instruction* I: ToRemove)
    I->eraseFromParent();

  if (Args.size() == 0) {
    return ToRemove.size() > 0;
  }

  auto& Ctx = F->getContext();

  // Gather argument types
  DenseMap<unsigned, unsigned> ArgsTyIdx;
  SmallVector<Type*, 4> FuncArgs;
  FuncArgs.reserve(Args.size());
  for (auto& IdxGEPs: Args) {
    auto& GEPs = IdxGEPs.second;
    auto* PtrTy = cast<PointerType>(getGEPStackScalarFinalPtr(*GEPs.begin())->getType());
    auto V = FuncArgs.size();
    ArgsTyIdx[IdxGEPs.first] = V;
    FuncArgs.push_back(PtrTy->getElementType());
  }

  FunctionType* NewFTy = FunctionType::get(F->getReturnType(), FuncArgs, false);
  Function* NewF = Function::Create(NewFTy, GlobalValue::ExternalLinkage, F->getName(), F->getParent());
  ValueToValueMapTy VTVM;
  SmallVector<ReturnInst*,1> Returns;
  CloneFunctionInto(NewF, F, VTVM, true, Returns, "");

  for (auto& IdxGEPs: Args) {
    auto& GEPs = IdxGEPs.second;
    auto ArgIdx = ArgsTyIdx[IdxGEPs.first];
    auto* EltTy = FuncArgs[ArgIdx];
    IRBuilder<> IRB(&*NewF->getEntryBlock().getFirstInsertionPt());
    auto* Alloca = IRB.CreateAlloca(EltTy);
    IRB.CreateStore(NewF->arg_begin()+ArgIdx, Alloca);

    const auto Idx = IdxGEPs.first;
    if (Idx >= 32768) {
      // This is a stack argument
    }
    for (auto* GEP: GEPs) {
      Value* VPtr = VTVM[getGEPStackScalarFinalPtr(GEP)];
      Instruction* IPtr = cast<Instruction>(VPtr);
      if (VPtr->getType() != Alloca->getType()) {
        auto* BC = IRBuilder<>{IPtr}.CreateBitCast(Alloca, VPtr->getType());
        IPtr->replaceAllUsesWith(BC);
      }
      else {
        IPtr->replaceAllUsesWith(Alloca);
      }
      IPtr->eraseFromParent();
    }
  }

  NewF->takeName(F);
  F->eraseFromParent();

  return true;
}

} // anonymous

namespace llvm {

bool ABIDecodePass::runImpl(Module& M) {
  // TODO: is this really true about the CPU flags?
  static const char* ClobberedRegs[] = {
    "ECX","EDX","af","pf","zf","of","cf","nf"};

  for (const char* CR: ClobberedRegs) {
    auto* Reg = miasmdec::GetRegisterGV(M, CR);
    if (!Reg) {
      LLVM_DEBUG(dbgs() << "no usage of the register " << CR << "\n");
      continue;
    }
    SmallVector<Value*, 8> TmpUsers(Reg->user_begin(), Reg->user_end());
    for (auto* U: TmpUsers) {
      if (auto* LI = dyn_cast<StoreInst>(U)) {
        LI->eraseFromParent();
      }
    }
  }

  // Gather all usages of g_stack per functions
  GlobalVariable* GVStack = miasmdec::GetStackGV(M);
  if (!GVStack) {
    return false;
  }

  DenseMap<Function*, LoadInst*> Stacks;
  for (auto* U: GVStack->users()) {
    if (auto* LI = dyn_cast<LoadInst>(U)) {
      auto* F = LI->getParent()->getParent();
      Stacks[F] = LI;
    }
    else {
      LLVM_DEBUG(dbgs() << *U << "\n");
      llvm::report_fatal_error("unsupported user of the stack GV!");
    }
  }

  // For each function, promote stack values to arguments, depending on the ABI

  for (auto& Funcs: Stacks) {
    promoteStackToArgs(Funcs.first, Funcs.second);
  }
  
  return Stacks.size() > 0;
}

PreservedAnalyses ABIDecodePass::run(Module& M, ModuleAnalysisManager &) {
  if (!runImpl(M))
    return PreservedAnalyses::all();

  return PreservedAnalyses::none();
}

} // llvm

// Legacy pass registration

namespace {

struct ABIDecodeLegacyPass: public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  ABIDecodeLegacyPass():
    ModulePass(ID)
  {
    initializeABIDecodeLegacyPassPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module& M) override {
    return Impl.runImpl(M);
  }

private:
  ABIDecodePass Impl;
};

} // anonymous 

char ABIDecodeLegacyPass::ID = 0;
INITIALIZE_PASS(ABIDecodeLegacyPass, "abi-decode",
  "Promote stack and registers to argument/return values, and clobber registers (Miasm specific)", false, false)

ModulePass* llvm::createABIDecodePass() { return new ABIDecodeLegacyPass{}; }
