#include "llvm/Transforms/MiasmDec.h"
#include "llvm/Transforms/MiasmDec/ABIDecode.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "Stack.h"
#include "Tools.h"

#define DEBUG_TYPE "abi-decode"

using namespace llvm;

namespace {

bool promoteStackToArgs(Function* F, LoadInst* SP)
{
  return true;
}

} // anonymous

namespace llvm {

bool ABIDecodePass::runImpl(Module& M) {
  errs() << "coucou\n";

  // TODO: clobber registers
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
