#ifndef LLVM_TRANSFORMS_SCALAR_SCALARIZE_STACK_H
#define LLVM_TRANSFORMS_SCALAR_SCALARIZE_STACK_H

#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class ScalarizeStackPass: public PassInfoMixin<ScalarizeStackPass> {
public:
  PreservedAnalyses run(Module& M, ModuleAnalysisManager &AM);

  // Glue for old PM.
  bool runImpl(Module& M);
};

} // llvm

#endif // LLVM_TRANSFORMS_SCALAR_FLOAT2INT_H
