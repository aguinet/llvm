#ifndef LLVM_MIASM_DEC_ABI_DECODE_H
#define LLVM_MIASM_DEC_ABI_DECODE_H

#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class ABIDecodePass: public PassInfoMixin<ABIDecodePass> {
public:
  PreservedAnalyses run(Module& M, ModuleAnalysisManager &AM);

  // Glue for old PM.
  bool runImpl(Module& M);
};

} // llvm

#endif
