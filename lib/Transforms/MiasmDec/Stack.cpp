#include "llvm/IR/Module.h"
#include "Stack.h"

using namespace llvm;

static const char* StackGVName = "g_stack";

GlobalVariable* miasmdec::GetStackGV(Module& M)
{
  return cast_or_null<GlobalVariable>(M.getNamedValue(StackGVName));
}

GlobalVariable* miasmdec::GetOrCreateStackGV(Module& M)
{
  auto* Ret = GetStackGV(M);
  if (Ret) {
    return Ret;
  }
  auto& Ctx = M.getContext();
  Ret = new GlobalVariable(M, Type::getInt8PtrTy(Ctx), false, GlobalVariable::ExternalLinkage, nullptr, StackGVName);
  return Ret;
}

