//===-- Little64ELFObjectWriter.cpp - Little64 ELF Object Writer ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Little64MCTargetDesc.h"
#include "Little64FixupKinds.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {

class Little64ELFObjectWriter : public MCELFObjectTargetWriter {
public:
  explicit Little64ELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(/*Is64Bit=*/true, OSABI, ELF::EM_LITTLE64,
                                /*HasRelocationAddend=*/true) {}

  ~Little64ELFObjectWriter() override = default;

  unsigned getRelocType(const MCFixup &Fixup, const MCValue &Target,
                        bool IsPCRel) const override;

  bool needsRelocateWithSymbol(const MCValue & /*Val*/,
                               unsigned Type) const override {
    switch (Type) {
    case ELF::R_LITTLE64_ABS64:
    case ELF::R_LITTLE64_ABS32:
    case ELF::R_LITTLE64_GOT64:
    case ELF::R_LITTLE64_TLS_DTPMOD64:
    case ELF::R_LITTLE64_TLS_DTPREL64:
    case ELF::R_LITTLE64_TLS_TPREL64:
    case ELF::R_LITTLE64_TLS_GOTTPREL:
      return true;
    default:
      return false;
    }
  }
};

} // end anonymous namespace

unsigned Little64ELFObjectWriter::getRelocType(const MCFixup &Fixup,
                                               const MCValue &Target,
                                               bool IsPCRel) const {
  MCFixupKind Kind = Fixup.getKind();

  // If the specifier encodes a relocation type directly (e.g. @got, @tprel),
  // use it for 8-byte data fixups.
  auto Spec = Target.getSpecifier();
  if (Spec) {
    switch (Spec) {
    case ELF::R_LITTLE64_GOT64:
    case ELF::R_LITTLE64_TLS_TPREL64:
    case ELF::R_LITTLE64_TLS_GOTTPREL:
      if (Kind == FK_Data_8)
        return Spec;
      llvm_unreachable("specifier only valid with .quad directive");
    default:
      break;
    }
  }

  // Standard data relocations.
  if (Kind == FK_Data_8)
    return ELF::R_LITTLE64_ABS64;
  if (Kind == FK_Data_4)
    return IsPCRel ? ELF::R_LITTLE64_PCREL32 : ELF::R_LITTLE64_ABS32;

  switch (Kind) {
  case (MCFixupKind)Little64::FK_Little64_PCREL6:
    return ELF::R_LITTLE64_PCREL6;
  case (MCFixupKind)Little64::FK_Little64_PCREL10:
    return ELF::R_LITTLE64_PCREL10;
  case (MCFixupKind)Little64::FK_Little64_PCREL13:
    return ELF::R_LITTLE64_PCREL13;
  case (MCFixupKind)Little64::FK_Little64_GOT64:
    return ELF::R_LITTLE64_GOT64;
  case (MCFixupKind)Little64::FK_Little64_TLS_TPREL64:
    return ELF::R_LITTLE64_TLS_TPREL64;
  default:
    llvm_unreachable("Unsupported fixup kind for Little64 ELF relocation");
  }
}

std::unique_ptr<MCObjectTargetWriter>
llvm::createLittle64ELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<Little64ELFObjectWriter>(OSABI);
}
