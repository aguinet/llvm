//===-- FunctionPass.cpp - Out-of-tree MachineIR pass example -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <llvm/CodeGen/MIRPrinter.h>
#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/CodeGen/RegisterMIRPasses.h>
#include <llvm/CodeGen/TargetPassConfig.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace {
struct MIRPrinter : public MachineFunctionPass {
  static char ID;

  MIRPrinter() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    printMIR(errs(), MF);
    return true;
  }
};

char MIRPrinter::ID = 0;
} // namespace

MachineFunctionPass *createMIRPrinterPass() { return new MIRPrinter{}; }

static void callback(TargetPassConfig const &, legacy::PassManagerBase &PM) {
  PM.add(createMIRPrinterPass());
}

static RegisterMIRPasses Register(TargetPassConfig::EP_OptimizerLast, callback);
