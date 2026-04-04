; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Regression coverage for CTLZ/CTTZ/CTPOP on i64 (ISD::CTLZ, ISD::CTTZ,
; ISD::CTPOP). These default to Legal in LLVM's base TargetLowering, so they
; must be explicitly registered as Expand to prevent "Cannot select" crashes.

; CTLZ expands to a leading-zero fill (SRLI+OR) followed by a popcount.
; CHECK-LABEL: test_ctlz:
; CHECK: SRLI
; CHECK: OR
; CHECK: MOVE R14, PC
define i64 @test_ctlz(i64 %x) {
  %r = call i64 @llvm.ctlz.i64(i64 %x, i1 false)
  ret i64 %r
}

; CHECK-LABEL: test_ctlz_zero_undef:
; CHECK: SRLI
; CHECK: OR
; CHECK: MOVE R14, PC
define i64 @test_ctlz_zero_undef(i64 %x) {
  %r = call i64 @llvm.ctlz.i64(i64 %x, i1 true)
  ret i64 %r
}

; CTTZ expands via bit-twiddling: isolate lowest set bit, then popcount.
; The XOR with all-ones mask is characteristic of the CTTZ expansion.
; CHECK-LABEL: test_cttz:
; CHECK: XOR
; CHECK: AND
; CHECK: MOVE R14, PC
define i64 @test_cttz(i64 %x) {
  %r = call i64 @llvm.cttz.i64(i64 %x, i1 false)
  ret i64 %r
}

; CHECK-LABEL: test_cttz_zero_undef:
; CHECK: AND
; CHECK: MOVE R14, PC
define i64 @test_cttz_zero_undef(i64 %x) {
  %r = call i64 @llvm.cttz.i64(i64 %x, i1 true)
  ret i64 %r
}

; CTPOP expands to a parallel bit-counting sequence (SRLI + AND + ADD).
; CHECK-LABEL: test_ctpop:
; CHECK: SRLI
; CHECK: AND
; CHECK: ADD
; CHECK: MOVE R14, PC
define i64 @test_ctpop(i64 %x) {
  %r = call i64 @llvm.ctpop.i64(i64 %x)
  ret i64 %r
}

declare i64 @llvm.ctlz.i64(i64, i1 immarg)
declare i64 @llvm.cttz.i64(i64, i1 immarg)
declare i64 @llvm.ctpop.i64(i64)
