#ifndef LLVM_MIASM_DEC_TOOLS_H
#define LLVM_MIASM_DEC_TOOLS_H

namespace llvm {
namespace miasmdec {

LoadInst* GetRegister(Function* F, const char* Reg);
bool ClobberRegister(LoadInst* Reg);
GlobalVariable* GetRegisterGV(Module& M, const char* Reg);

} // miasmdec
} // llvm

#endif
