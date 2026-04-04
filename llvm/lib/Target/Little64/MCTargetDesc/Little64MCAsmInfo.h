//===-- Little64MCAsmInfo.h - Little64 asm properties ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LITTLE64_MCTARGETDESC_LITTLE64MCASMINFO_H
#define LLVM_LIB_TARGET_LITTLE64_MCTARGETDESC_LITTLE64MCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;
class MCSpecifierExpr;

class Little64MCAsmInfo : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit Little64MCAsmInfo(const Triple &TheTriple,
                              const MCTargetOptions &Options);
  void printSpecifierExpr(raw_ostream &OS,
                          const MCSpecifierExpr &Expr) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_LITTLE64_MCTARGETDESC_LITTLE64MCASMINFO_H
