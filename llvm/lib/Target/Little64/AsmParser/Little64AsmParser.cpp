//===-- Little64AsmParser.cpp - Little64 Assembly Parser ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Little64MCTargetDesc.h"
#include "TargetInfo/Little64TargetInfo.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {

class Little64Operand : public MCParsedAsmOperand {
  enum KindTy { Token, Register, Immediate, Memory } Kind;

  struct MemOp {
    unsigned BaseReg;
    const MCExpr *Offset;
  };

  std::string TokenString;
  unsigned RegNum;
  const MCExpr *ImmVal;
  MemOp Mem;

  SMLoc StartLoc, EndLoc;

public:
  Little64Operand(KindTy K) : Kind(K) {}

  bool isToken() const override { return Kind == Token; }
  bool isReg() const override { return Kind == Register; }
  bool isImm() const override { return Kind == Immediate; }
  bool isMem() const override { return Kind == Memory; }

  MCRegister getReg() const override { return MCRegister(RegNum); }
  const MCExpr *getImm() const { return ImmVal; }

  StringRef getToken() const {
    return TokenString;
  }

  static std::unique_ptr<Little64Operand> createToken(StringRef Str, SMLoc S) {
    auto Op = std::make_unique<Little64Operand>(Token);
    Op->TokenString = Str.str();
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }

  static std::unique_ptr<Little64Operand> createReg(unsigned RegNo, SMLoc S,
                                                    SMLoc E) {
    auto Op = std::make_unique<Little64Operand>(Register);
    Op->RegNum = RegNo;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<Little64Operand> createImm(const MCExpr *Val, SMLoc S,
                                                    SMLoc E) {
    auto Op = std::make_unique<Little64Operand>(Immediate);
    Op->ImmVal = Val;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<Little64Operand> createMem(unsigned BaseReg,
                                                    const MCExpr *Offset,
                                                    SMLoc S, SMLoc E) {
    auto Op = std::make_unique<Little64Operand>(Memory);
    Op->Mem.BaseReg = BaseReg;
    Op->Mem.Offset = Offset;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::createReg(getReg()));
  }

  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    if (auto *CE = dyn_cast<MCConstantExpr>(getImm()))
      Inst.addOperand(MCOperand::createImm(CE->getValue()));
    else
      Inst.addOperand(MCOperand::createExpr(getImm()));
  }

  void addMemOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands for mem_ri!");
    Inst.addOperand(MCOperand::createReg(Mem.BaseReg));
    if (auto *CE = dyn_cast<MCConstantExpr>(Mem.Offset))
      Inst.addOperand(MCOperand::createImm(CE->getValue()));
    else
      Inst.addOperand(MCOperand::createExpr(Mem.Offset));
  }

  SMLoc getStartLoc() const override { return StartLoc; }
  SMLoc getEndLoc() const override { return EndLoc; }

  void print(raw_ostream &OS, const MCAsmInfo &MAI) const override {
    switch (Kind) {
    case Token:     OS << "Token: " << getToken(); break;
    case Register:  OS << "Reg: " << getReg(); break;
    case Immediate: OS << "Imm"; break;
    case Memory:
      OS << "Mem: [" << Mem.BaseReg << " + (offset)]";
      break;
    }
  }
};

class Little64AsmParser : public MCTargetAsmParser {
  MCAsmParser &Parser;
  std::string CurrentMnemonic;

#define GET_ASSEMBLER_HEADER
#include "Little64GenAsmMatcher.inc"

  bool matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;

  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc, SMLoc &EndLoc) override;
  ParseStatus tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                               SMLoc &EndLoc) override;

  bool parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;

  bool parseOperand(OperandVector &Operands);

  bool parseMemoryOperand(OperandVector &Operands);

public:
  Little64AsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                    const MCInstrInfo &MII, const MCTargetOptions &Options)
      : MCTargetAsmParser(Options, STI, MII), Parser(Parser) {
    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }
};

} // end anonymous namespace

bool Little64AsmParser::matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                                OperandVector &Operands,
                                                MCStreamer &Out,
                                                uint64_t &ErrorInfo,
                                                bool MatchingInlineAsm) {
  MCInst Inst;
  unsigned MatchResult =
      MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm);

  switch (MatchResult) {
  case Match_Success: {
    auto isLSMemRIOpcode = [](unsigned Opc) {
      switch (Opc) {
      case Little64::LOAD:
      case Little64::STORE:
      case Little64::BYTE_LOAD:
      case Little64::BYTE_STORE:
      case Little64::SHORT_LOAD:
      case Little64::SHORT_STORE:
      case Little64::WORD_LOAD:
      case Little64::WORD_STORE:
        return true;
      default:
        return false;
      }
    };

    auto isShiftImmOpcode = [](unsigned Opc) {
      switch (Opc) {
      case Little64::SLLI:
      case Little64::SRLI:
      case Little64::SRAI:
        return true;
      default:
        return false;
      }
    };

    // LS register-indirect memory ops encode a 2-bit halfword-scaled offset.
    // Accept only byte offsets {0, 2, 4, 6} to avoid silent truncation.
    if (isLSMemRIOpcode(Inst.getOpcode())) {
      const MCOperand &OffOp = Inst.getOperand(Inst.getNumOperands() - 1);
      if (!OffOp.isImm())
        return Error(IDLoc, "invalid operand for instruction");

      int64_t Off = OffOp.getImm();
      if (Off < 0 || Off > 6 || (Off & 1) != 0)
        return Error(IDLoc, "invalid operand for instruction");
    }

    // GP immediate shifts encode only a 4-bit count in the Rs1 field.
    if (isShiftImmOpcode(Inst.getOpcode())) {
      const MCOperand *ShamtOp = nullptr;
      for (const MCOperand &Op : Inst) {
        if (!Op.isImm())
          continue;
        ShamtOp = &Op;
        break;
      }

      if (!ShamtOp)
        return Error(IDLoc, "invalid operand for instruction");

      int64_t Shamt = ShamtOp->getImm();
      if (Shamt < 0 || Shamt > 15)
        return Error(IDLoc, "invalid operand for instruction");
    }

    // PUSH Rs1: the Rd (stack pointer) operand is implicit in assembly
    // syntax but must be R13 in the encoding.  The auto-matcher leaves it
    // unset, so patch it here.
    // MCInst order: [0]=Rd(out), [1]=Rs1(in), [2]=Rd_in(in, tied to Rd).
    if (Inst.getOpcode() == Little64::PUSH) {
      unsigned Rs1 = Inst.getOperand(1).getReg();
      Inst.clear();
      Inst.setOpcode(Little64::PUSH);
      Inst.addOperand(MCOperand::createReg(Little64::R13));
      Inst.addOperand(MCOperand::createReg(Rs1));
      Inst.addOperand(MCOperand::createReg(Little64::R13));
      Inst.setLoc(IDLoc);
      Out.emitInstruction(Inst, getSTI());
      return false;
    }

    // POP Rs1: same issue — Rd (stack pointer) must be R13.
    // MCInst order: [0]=Rs1(out), [1]=Rd(out), [2]=Rd_in(in, tied to Rd).
    if (Inst.getOpcode() == Little64::POP) {
      unsigned Rs1 = Inst.getOperand(0).getReg();
      Inst.clear();
      Inst.setOpcode(Little64::POP);
      Inst.addOperand(MCOperand::createReg(Rs1));
      Inst.addOperand(MCOperand::createReg(Little64::R13));
      Inst.addOperand(MCOperand::createReg(Little64::R13));
      Inst.setLoc(IDLoc);
      Out.emitInstruction(Inst, getSTI());
      return false;
    }

    if (Inst.getOpcode() == Little64::CALL) {
      const MCOperand &TargetOp = Inst.getOperand(0);
      if (!TargetOp.isReg())
        return Error(IDLoc, "CALL expects a register target");

      Out.emitInstruction(
          MCInstBuilder(Little64::MOVErr)
              .addReg(Little64::R14)
              .addReg(Little64::R15)
              .addImm(2),
          getSTI());
      Out.emitInstruction(
          MCInstBuilder(Little64::MOVErr)
              .addReg(Little64::R15)
              .addReg(TargetOp.getReg())
              .addImm(0),
          getSTI());
      return false;
    }

    if (Inst.getOpcode() == Little64::RET) {
      Out.emitInstruction(
          MCInstBuilder(Little64::MOVErr)
              .addReg(Little64::R15)
              .addReg(Little64::R14)
              .addImm(0),
          getSTI());
      return false;
    }

    if (Inst.getOpcode() == Little64::LDI64) {
      unsigned Rd = Inst.getOperand(0).getReg();
      const MCOperand &ImmOp = Inst.getOperand(1);

      if (ImmOp.isImm()) {
        int64_t Imm = ImmOp.getImm();
        if (Imm == static_cast<int64_t>(static_cast<int32_t>(Imm))) {
          uint64_t UImm = static_cast<uint64_t>(Imm);
          uint8_t B0 = (UImm >> 0) & 0xFF;
          uint8_t B1 = (UImm >> 8) & 0xFF;
          uint8_t B2 = (UImm >> 16) & 0xFF;
          uint8_t B3 = (UImm >> 24) & 0xFF;

          Out.emitInstruction(
              MCInstBuilder(Little64::LDI).addReg(Rd).addImm(B0), getSTI());
          if (B1 || B2 || B3)
            Out.emitInstruction(MCInstBuilder(Little64::LDI_S1)
                                    .addReg(Rd)
                                    .addImm(B1)
                                    .addReg(Rd),
                                getSTI());
          if (B2 || B3)
            Out.emitInstruction(MCInstBuilder(Little64::LDI_S2)
                                    .addReg(Rd)
                                    .addImm(B2)
                                    .addReg(Rd),
                                getSTI());
          if (B3)
            Out.emitInstruction(MCInstBuilder(Little64::LDI_S3)
                                    .addReg(Rd)
                                    .addImm(B3)
                                    .addReg(Rd),
                                getSTI());
        } else {
          Out.emitInstruction(MCInstBuilder(Little64::JMP).addImm(4), getSTI());
          Out.emitIntValue(static_cast<uint64_t>(Imm), 8);
          Out.emitInstruction(
              MCInstBuilder(Little64::LOAD_PCREL).addReg(Rd).addImm(-5),
              getSTI());
        }
      } else {
        Out.emitInstruction(MCInstBuilder(Little64::JMP).addImm(4), getSTI());
        Out.emitValue(ImmOp.getExpr(), 8);
        Out.emitInstruction(
            MCInstBuilder(Little64::LOAD_PCREL).addReg(Rd).addImm(-5), getSTI());
      }
      return false;
    }
    Inst.setLoc(IDLoc);
    Out.emitInstruction(Inst, getSTI());
    return false;
  }
  case Match_MnemonicFail:
    return Error(IDLoc, "unknown instruction mnemonic");
  case Match_InvalidOperand:
    return Error(IDLoc, "invalid operand for instruction");
  default:
    return Error(IDLoc, "unknown match failure");
  }
}

bool Little64AsmParser::parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                      SMLoc &EndLoc) {
  if (tryParseRegister(Reg, StartLoc, EndLoc).isSuccess())
    return false;
  return true;
}

ParseStatus Little64AsmParser::tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                                SMLoc &EndLoc) {
  const AsmToken &Tok = Parser.getTok();
  StartLoc = Tok.getLoc();
  EndLoc = Tok.getEndLoc();

  if (Tok.isNot(AsmToken::Identifier))
    return ParseStatus::NoMatch;

  std::string Name = Tok.getString().lower();
  unsigned RegNo = StringSwitch<unsigned>(Name)
                       .Case("r0", Little64::R0)
                       .Case("r1", Little64::R1)
                       .Case("r2", Little64::R2)
                       .Case("r3", Little64::R3)
                       .Case("r4", Little64::R4)
                       .Case("r5", Little64::R5)
                       .Case("r6", Little64::R6)
                       .Case("r7", Little64::R7)
                       .Case("r8", Little64::R8)
                       .Case("r9", Little64::R9)
                       .Case("r10", Little64::R10)
                       .Case("r11", Little64::R11)
                       .Case("r12", Little64::R12)
                       .Case("r13", Little64::R13)
                       .Case("sp", Little64::R13)
                       .Case("r14", Little64::R14)
                       .Case("lr", Little64::R14)
                       .Case("r15", Little64::R15)
                       .Case("pc", Little64::R15)
                       .Default(0);

  if (RegNo == 0)
    return ParseStatus::NoMatch;

  Reg = RegNo;
  Parser.Lex();
  return ParseStatus::Success;
}

bool Little64AsmParser::parseInstruction(ParseInstructionInfo &Info,
                                         StringRef Name, SMLoc NameLoc,
                                         OperandVector &Operands) {
  std::string Mnemonic = Name.lower();
  while (Parser.getTok().is(AsmToken::Dot)) {
    Mnemonic += ".";
    Parser.Lex();
    if (Parser.getTok().is(AsmToken::Identifier)) {
      Mnemonic += Parser.getTok().getString().lower();
      Parser.Lex();
    }
  }
  static std::string MnemonicStorage;
  MnemonicStorage = Mnemonic;
  CurrentMnemonic = Mnemonic;
  Operands.push_back(Little64Operand::createToken(MnemonicStorage, NameLoc));

  while (Parser.getTok().isNot(AsmToken::EndOfStatement)) {
    if (Parser.getTok().is(AsmToken::Comma)) {
      Parser.Lex();
      continue;
    }
    if (parseOperand(Operands))
      return true;
  }

  Parser.Lex();
  return false;
}

bool Little64AsmParser::parseMemoryOperand(OperandVector &Operands) {
  SMLoc S = Parser.getTok().getLoc();
  Parser.Lex(); // Eat '['

  MCRegister BaseReg;
  SMLoc RegS, RegE;
  if (tryParseRegister(BaseReg, RegS, RegE).isFailure())
    return Error(RegS, "expected register in memory operand");

  const MCExpr *Offset = nullptr;
  if (Parser.getTok().is(AsmToken::Plus)) {
    Parser.Lex(); // Eat '+'
    if (getParser().parseExpression(Offset))
      return true;
  } else {
    Offset = MCConstantExpr::create(0, getContext());
  }

  if (Parser.getTok().isNot(AsmToken::RBrac))
    return Error(Parser.getTok().getLoc(), "expected ']' in memory operand");

  SMLoc E = Parser.getTok().getLoc();
  Parser.Lex(); // Eat ']'

  Operands.push_back(Little64Operand::createMem(BaseReg, Offset, S, E));
  return false;
}

bool Little64AsmParser::parseOperand(OperandVector &Operands) {
  MCRegister Reg;
  SMLoc S, E;
  const AsmToken &Tok = Parser.getTok();

  if (Tok.is(AsmToken::LBrac))
    return parseMemoryOperand(Operands);

  if (Tok.is(AsmToken::Hash)) {
    // LDI64 historically accepts an optional '#' prefix in tests/tooling.
    // Consume it here so both `LDI64 imm, Rd` and `LDI64 #imm, Rd` work.
    if (CurrentMnemonic == "ldi64") {
      Parser.Lex();
      return false;
    }
    Operands.push_back(Little64Operand::createToken("#", Tok.getLoc()));
    Parser.Lex();
    return false;
  }
  if (Tok.is(AsmToken::At)) {
    Operands.push_back(Little64Operand::createToken("@", Tok.getLoc()));
    Parser.Lex();
    return false;
  }

  if (Tok.is(AsmToken::Identifier)) {
    if (tryParseRegister(Reg, S, E).isSuccess()) {
      Operands.push_back(Little64Operand::createReg(Reg, S, E));
      return false;
    }
  }

  const MCExpr *Val;
  S = Tok.getLoc();
  if (getParser().parseExpression(Val))
    return true;
  E = Parser.getTok().getLoc();
  Operands.push_back(Little64Operand::createImm(Val, S, E));
  return false;
}

#define GET_MATCHER_IMPLEMENTATION
#include "Little64GenAsmMatcher.inc"

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeLittle64AsmParser() {
  RegisterMCAsmParser<Little64AsmParser> X(getTheLittle64Target());
}
