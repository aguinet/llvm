#ifndef LLVM_MIASM_DEC_STACK_H
#define LLVM_MIASM_DEC_STACK_H

namespace llvm {

class GlobalVariable;
class Module;

namespace miasmdec {

GlobalVariable* GetOrCreateStackGV(Module&);
GlobalVariable* GetStackGV(Module&);

} // miasmdec

} // llvm

#endif
