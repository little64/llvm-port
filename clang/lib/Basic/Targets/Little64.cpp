//===--- Little64.cpp - Implement Little64 target feature support ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements Little64TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "Little64.h"
#include "clang/Basic/MacroBuilder.h"
#include "llvm/ADT/StringSwitch.h"

using namespace clang;
using namespace clang::targets;

void Little64TargetInfo::getTargetDefines(const LangOptions &Opts,
                                          MacroBuilder &Builder) const {
  Builder.defineMacro("__little64__");
}

ArrayRef<const char *> Little64TargetInfo::getGCCRegNames() const {
  static const char *const GCCRegNames[] = {
      "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
      "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"
  };
  return llvm::ArrayRef(GCCRegNames);
}

ArrayRef<TargetInfo::GCCRegAlias> Little64TargetInfo::getGCCRegAliases() const {
  static const TargetInfo::GCCRegAlias GCCRegAliases[] = {
      {{"r0", "R0"}, "r0"},
      {{"r1", "R1"}, "r1"},
      {{"r2", "R2"}, "r2"},
      {{"r3", "R3"}, "r3"},
      {{"r4", "R4"}, "r4"},
      {{"r5", "R5"}, "r5"},
      {{"r6", "R6"}, "r6"},
      {{"r7", "R7"}, "r7"},
      {{"r8", "R8"}, "r8"},
      {{"r9", "R9"}, "r9"},
      {{"r10", "R10"}, "r10"},
      {{"r11", "R11"}, "r11"},
      {{"r12", "R12"}, "r12"},
      {{"r13", "R13", "sp", "SP"}, "r13"},
      {{"r14", "R14", "lr", "LR"}, "r14"},
      {{"r15", "R15", "pc", "PC"}, "r15"},
  };
  return llvm::ArrayRef(GCCRegAliases);
}
