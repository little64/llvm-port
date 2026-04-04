//=== lib/builtins/little64/fp_mode.c - FP mode utilities -----*- C -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Little-64 has no floating-point hardware; always return default rounding
// mode and treat raise-inexact as a no-op.
//
//===----------------------------------------------------------------------===//
#include "../fp_mode.h"

CRT_FE_ROUND_MODE __fe_getround(void) {
  return CRT_FE_TONEAREST;
}

int __fe_raise_inexact(void) {
  return 0;
}
