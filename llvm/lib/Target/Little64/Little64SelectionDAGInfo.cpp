//===-- Little64SelectionDAGInfo.cpp - Little64 SelectionDAG Info ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Little64SelectionDAGInfo.h"

#define GET_SDNODE_DESC
#include "Little64GenSDNodeInfo.inc"

using namespace llvm;

Little64SelectionDAGInfo::Little64SelectionDAGInfo()
    : SelectionDAGGenTargetInfo(Little64GenSDNodeInfo) {}

Little64SelectionDAGInfo::~Little64SelectionDAGInfo() = default;
