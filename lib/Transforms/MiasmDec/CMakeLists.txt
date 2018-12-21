add_llvm_library(LLVMMiasmDec
  MiasmDec.cpp
  Stack.cpp
  Tools.cpp

  # Passes
  ScalarizeStack.cpp
  ABIDecode.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/MiasmDec

  DEPENDS
  intrinsics_gen
  )
