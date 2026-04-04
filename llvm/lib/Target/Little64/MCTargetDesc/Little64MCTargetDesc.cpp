//===-- Little64MCTargetDesc.cpp - Little64 Target Descriptions -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides Little64 specific target descriptions.
//
//===----------------------------------------------------------------------===//

#include "Little64MCTargetDesc.h"
#include "Little64InstPrinter.h"
#include "Little64MCAsmInfo.h"
#include "TargetInfo/Little64TargetInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/TargetParser/Triple.h"
#include <string>

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "Little64GenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "Little64GenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "Little64GenRegisterInfo.inc"

using namespace llvm;

static MCInstrInfo *createLittle64MCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitLittle64MCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createLittle64MCRegisterInfo(const Triple & /*TT*/) {
  MCRegisterInfo *X = new MCRegisterInfo();
  // R14 = LR (link register, return address register).
  // R15 = PC (program counter).
  InitLittle64MCRegisterInfo(X, Little64::R14, 0, 0, Little64::R15);
  return X;
}

static MCSubtargetInfo *
createLittle64MCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
  std::string CPUName = std::string(CPU);
  if (CPUName.empty())
    CPUName = "generic";
  return createLittle64MCSubtargetInfoImpl(TT, CPUName, CPUName, FS);
}

static MCStreamer *createMCStreamer(const Triple &T, MCContext &Context,
                                   std::unique_ptr<MCAsmBackend> &&MAB,
                                   std::unique_ptr<MCObjectWriter> &&OW,
                                   std::unique_ptr<MCCodeEmitter> &&Emitter) {
  if (!T.isOSBinFormatELF())
    llvm_unreachable("OS not supported");
  return createELFStreamer(Context, std::move(MAB), std::move(OW),
                           std::move(Emitter));
}

static MCInstPrinter *
createLittle64MCInstPrinter(const Triple & /*T*/, unsigned SyntaxVariant,
                            const MCAsmInfo &MAI, const MCInstrInfo &MII,
                            const MCRegisterInfo &MRI) {
  if (SyntaxVariant == 0)
    return new Little64InstPrinter(MAI, MII, MRI);
  return nullptr;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeLittle64TargetMC() {
  // Register the MC asm info.
  RegisterMCAsmInfo<Little64MCAsmInfo> X(getTheLittle64Target());

  // Register the MC instruction info.
  TargetRegistry::RegisterMCInstrInfo(getTheLittle64Target(),
                                      createLittle64MCInstrInfo);

  // Register the MC register info.
  TargetRegistry::RegisterMCRegInfo(getTheLittle64Target(),
                                    createLittle64MCRegisterInfo);

  // Register the MC subtarget info.
  TargetRegistry::RegisterMCSubtargetInfo(getTheLittle64Target(),
                                          createLittle64MCSubtargetInfo);

  // Register the MC code emitter.
  TargetRegistry::RegisterMCCodeEmitter(getTheLittle64Target(),
                                        createLittle64MCCodeEmitter);

  // Register the ASM backend.
  TargetRegistry::RegisterMCAsmBackend(getTheLittle64Target(),
                                       createLittle64AsmBackend);

  // Register the MCInstPrinter.
  TargetRegistry::RegisterMCInstPrinter(getTheLittle64Target(),
                                        createLittle64MCInstPrinter);

  // Register the ELF streamer.
  TargetRegistry::RegisterELFStreamer(getTheLittle64Target(), createMCStreamer);
}
