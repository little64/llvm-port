//===-- Little64ExpandPseudos.h - Little64 Pseudo Expansion -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LITTLE64_LITTLE64EXPANDPSEUDOS_H
#define LLVM_LIB_TARGET_LITTLE64_LITTLE64EXPANDPSEUDOS_H

namespace llvm {

class FunctionPass;
class PassRegistry;

FunctionPass *createLittle64ExpandPseudosPass();
FunctionPass *createLittle64ExpandControlFlowPseudosPass();
FunctionPass *createLittle64ExpandAddressPseudosPass();

void initializeLittle64ExpandPseudosPass(PassRegistry &);

} // namespace llvm

#endif // LLVM_LIB_TARGET_LITTLE64_LITTLE64EXPANDPSEUDOS_H
