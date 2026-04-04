//===-- Little64TargetInfo.cpp - Little64 Target Implementation -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Little64TargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

Target &llvm::getTheLittle64Target() {
  static Target TheLittle64Target;
  return TheLittle64Target;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeLittle64TargetInfo() {
  RegisterTarget<Triple::little64> X(getTheLittle64Target(), "little64",
                                     "Little-64 64-bit RISC ISA", "Little64");
}
