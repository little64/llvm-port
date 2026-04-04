//===-- Little64Subtarget.cpp - Little64 Subtarget Information ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Little64Subtarget.h"
#include "Little64.h"
#include "llvm/CodeGen/LibcallLoweringInfo.h"
#include "llvm/IR/RuntimeLibcalls.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"

#define DEBUG_TYPE "little64-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "Little64GenSubtargetInfo.inc"

using namespace llvm;

Little64Subtarget::Little64Subtarget(const Triple &TargetTriple, StringRef CPU,
                                       StringRef FS, const TargetMachine &TM,
                                       const TargetOptions &Options,
                                       CodeModel::Model CM,
                                       CodeGenOptLevel OL)
    : Little64GenSubtargetInfo(TargetTriple, CPU, /*TuneCPU=*/CPU, FS),
      InstrInfo(*this), FrameLowering(),
      TLInfo(TM, *this) {
  initializeSubtargetDependencies(CPU, FS);
}

Little64Subtarget &
Little64Subtarget::initializeSubtargetDependencies(StringRef CPU, StringRef FS) {
  std::string CPUName = std::string(CPU);
  if (CPUName.empty())
    CPUName = "generic";
  ParseSubtargetFeatures(CPUName, CPUName, FS);
  return *this;
}

void Little64Subtarget::initLibcallLoweringInfo(LibcallLoweringInfo &Info) const {
  // Register standard GCC/compiler-rt libcalls for integer arithmetic that
  // Little-64 has no hardware support for.
  static const struct { RTLIB::Libcall Op; RTLIB::LibcallImpl Impl; } Calls[] = {
    { RTLIB::MEMCPY,   RTLIB::impl_memcpy   },
    { RTLIB::MEMMOVE,  RTLIB::impl_memmove  },
    { RTLIB::MEMSET,   RTLIB::impl_memset   },

    { RTLIB::CTLZ_I32, RTLIB::impl___clzsi2 },
    { RTLIB::CTLZ_I64, RTLIB::impl___clzdi2 },
    { RTLIB::CTPOP_I32, RTLIB::impl___popcountsi2 },
    { RTLIB::CTPOP_I64, RTLIB::impl___popcountdi2 },

    { RTLIB::ADD_F32,  RTLIB::impl___addsf3 },
    { RTLIB::ADD_F64,  RTLIB::impl___adddf3 },
    { RTLIB::SUB_F32,  RTLIB::impl___subsf3 },
    { RTLIB::SUB_F64,  RTLIB::impl___subdf3 },
    { RTLIB::MUL_F32,  RTLIB::impl___mulsf3 },
    { RTLIB::MUL_F64,  RTLIB::impl___muldf3 },
    { RTLIB::DIV_F32,  RTLIB::impl___divsf3 },
    { RTLIB::DIV_F64,  RTLIB::impl___divdf3 },

    { RTLIB::FPTOSINT_F32_I32, RTLIB::impl___fixsfsi },
    { RTLIB::FPTOSINT_F32_I64, RTLIB::impl___fixsfdi },
    { RTLIB::FPTOSINT_F64_I32, RTLIB::impl___fixdfsi },
    { RTLIB::FPTOSINT_F64_I64, RTLIB::impl___fixdfdi },

    { RTLIB::FPTOUINT_F32_I32, RTLIB::impl___fixunssfsi },
    { RTLIB::FPTOUINT_F32_I64, RTLIB::impl___fixunssfdi },
    { RTLIB::FPTOUINT_F64_I32, RTLIB::impl___fixunsdfsi },
    { RTLIB::FPTOUINT_F64_I64, RTLIB::impl___fixunsdfdi },

    { RTLIB::SINTTOFP_I32_F32, RTLIB::impl___floatsisf },
    { RTLIB::SINTTOFP_I32_F64, RTLIB::impl___floatsidf },
    { RTLIB::SINTTOFP_I64_F32, RTLIB::impl___floatdisf },
    { RTLIB::SINTTOFP_I64_F64, RTLIB::impl___floatdidf },

    { RTLIB::UINTTOFP_I32_F32, RTLIB::impl___floatunsisf },
    { RTLIB::UINTTOFP_I32_F64, RTLIB::impl___floatunsidf },
    { RTLIB::UINTTOFP_I64_F32, RTLIB::impl___floatundisf },
    { RTLIB::UINTTOFP_I64_F64, RTLIB::impl___floatundidf },

    { RTLIB::MUL_I32,  RTLIB::impl___mulsi3  },
    { RTLIB::MUL_I64,  RTLIB::impl___muldi3  },
    { RTLIB::SDIV_I32, RTLIB::impl___divsi3  },
    { RTLIB::SDIV_I64, RTLIB::impl___divdi3  },
    { RTLIB::SREM_I32, RTLIB::impl___modsi3  },
    { RTLIB::SREM_I64, RTLIB::impl___moddi3  },
    { RTLIB::UDIV_I32, RTLIB::impl___udivsi3 },
    { RTLIB::UDIV_I64, RTLIB::impl___udivdi3 },
    { RTLIB::UREM_I32, RTLIB::impl___umodsi3 },
    { RTLIB::UREM_I64, RTLIB::impl___umoddi3 },
  };
  for (const auto &LC : Calls)
    Info.setLibcallImpl(LC.Op, LC.Impl);
}
