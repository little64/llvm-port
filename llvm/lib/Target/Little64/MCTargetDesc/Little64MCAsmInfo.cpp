//===-- Little64MCAsmInfo.cpp - Little64 asm properties ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Little64MCAsmInfo.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

void Little64MCAsmInfo::anchor() {}

Little64MCAsmInfo::Little64MCAsmInfo(const Triple & /*TheTriple*/,
                                     const MCTargetOptions &Options) {
  IsLittleEndian = true;
  CodePointerSize = CalleeSaveStackSlotSize = 8;
  InternalSymbolPrefix = ".L";
  WeakRefDirective = "\t.weak\t";
  ExceptionsType = ExceptionHandling::DwarfCFI;
  UsesELFSectionDirectiveForBSS = true;

  // Little-64 uses ';' as the comment character.
  CommentString = ";";

  // Little-64's ".int" directive emits a 32-bit word (4 bytes).
  Data32bitsDirective = "\t.int\t";
  // Little-64's ".quad" directive emits a 64-bit word (8 bytes).
  Data64bitsDirective = "\t.quad\t";

  // Little-64 assembler uses ".asciz" for null-terminated strings.
  AscizDirective = "\t.asciz\t";

  // Instructions are always 2 bytes (16-bit).
  MinInstAlignment = 2;
  MaxInstLength = 2;

  SupportsDebugInformation = true;

  // Register @specifier names for relocations used in PIC/TLS code.
  // The specifier values use ELF relocation type constants directly.
  initializeAtSpecifiers({
      {ELF::R_LITTLE64_GOT64, "got"},
      {ELF::R_LITTLE64_TLS_TPREL64, "tprel"},
  });
}

void Little64MCAsmInfo::printSpecifierExpr(raw_ostream &OS,
                                           const MCSpecifierExpr &Expr) const {
  printExpr(OS, *Expr.getSubExpr());
  OS << '@' << getSpecifierName(Expr.getSpecifier());
}
