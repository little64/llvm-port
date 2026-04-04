//===-- Little64SelectionDAGInfo.h - Little64 SelectionDAG Info -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LITTLE64_LITTLE64SELECTIONDAGINFO_H
#define LLVM_LIB_TARGET_LITTLE64_LITTLE64SELECTIONDAGINFO_H

#include "llvm/CodeGen/SDNodeInfo.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"

namespace llvm {

class Little64SelectionDAGInfo : public SelectionDAGGenTargetInfo {
public:
  Little64SelectionDAGInfo();
  ~Little64SelectionDAGInfo() override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_LITTLE64_LITTLE64SELECTIONDAGINFO_H
