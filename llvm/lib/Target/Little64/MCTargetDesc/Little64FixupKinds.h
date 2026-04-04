//===-- Little64FixupKinds.h - Little64 Fixup Entries -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LITTLE64_MCTARGETDESC_LITTLE64FIXUPKINDS_H
#define LLVM_LIB_TARGET_LITTLE64_MCTARGETDESC_LITTLE64FIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace Little64 {
enum Fixups {
  // 6-bit PC-relative offset for non-JUMP LS PC-relative instructions.
  // Bits [9:4] in the instruction word; offset is in instruction units (×2).
  FK_Little64_PCREL6 = FirstTargetFixupKind,

  // 10-bit PC-relative offset for JUMP.* conditional branch instructions.
  // Bits [9:0] in the instruction word; offset is in instruction units (×2).
  FK_Little64_PCREL10,

  // 13-bit PC-relative offset for unconditional JUMP instructions (format 111).
  // Bits [12:0] in the instruction word; offset is in instruction units (×2).
  FK_Little64_PCREL13,

  // 64-bit GOT-relative offset — used in literal slots to reference GOT entries.
  // Produces R_LITTLE64_GOT64 in the ELF object.
  FK_Little64_GOT64,

  // 64-bit TLS offset from thread pointer (Initial Exec model).
  // Produces R_LITTLE64_TLS_TPREL64 in the ELF object.
  FK_Little64_TLS_TPREL64,

  // Marker
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};
} // namespace Little64
} // namespace llvm

#endif // LLVM_LIB_TARGET_LITTLE64_MCTARGETDESC_LITTLE64FIXUPKINDS_H
