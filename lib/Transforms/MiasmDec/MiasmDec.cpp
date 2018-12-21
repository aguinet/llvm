//===-- Scalar.cpp --------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements common infrastructure for libLLVMScalarOpts.a, which
// implements several scalar transformations over the LLVM intermediate
// representation, including the C bindings for that library.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/MiasmDec.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

void llvm::initializeMiasmDec(PassRegistry &Registry) {
  initializeScalarizeStackLegacyPassPass(Registry);
  initializeABIDecodeLegacyPassPass(Registry);
}
