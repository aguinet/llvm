#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/ADT/SmallVector.h"

#include "Tools.h"

using namespace llvm;

bool miasmdec::ClobberRegister(LoadInst* Reg)
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

GlobalVariable* miasmdec::GetRegisterGV(Module& M, const char* Reg)
{
  return dyn_cast_or_null<GlobalVariable>(M.getNamedValue(Reg));
}

LoadInst* miasmdec::GetRegister(Function* F, const char* Reg)
{
  auto* GV = GetRegisterGV(*F->getParent(), Reg);
  if (!GV) {
    return nullptr;
  }
  for (auto* U: GV->users()) {
    if (auto* LI = dyn_cast<LoadInst>(U)) {
      if (LI->getParent()->getParent() == F) {
        return LI;
      }
    }
  }
  return nullptr;
}
