//===- Little64.td - Describe the Little64 Target Machine --*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

include "Little64Schedule.td"
include "Little64RegisterInfo.td"
include "Little64CallingConv.td"
include "Little64InstrFormats.td"
include "Little64InstrInfo.td"

def Little64InstrInfo : InstrInfo;

// Map all ptr_rc uses in standard pseudo-instructions to GPR (the 64-bit
// general-purpose register class).
defm : RemapAllTargetPseudoPointerOperands<GPR>;

//===----------------------------------------------------------------------===//
// Little64 processors supported
//===----------------------------------------------------------------------===//

def : ProcessorModel<"generic", Little64SchedModel, []>;

def Little64InstPrinter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
  bit isMCAsmWriter = 1;
}

//===----------------------------------------------------------------------===//
// Target declaration
//===----------------------------------------------------------------------===//

def Little64 : Target {
  let InstructionSet  = Little64InstrInfo;
  let AssemblyWriters = [Little64InstPrinter];
}
