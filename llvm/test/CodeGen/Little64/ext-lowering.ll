; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Regression coverage for integer extension lowering patterns. Zero-extend
; should materialize a mask-and operation while sign-extend should lower to
; shift-left + arithmetic-shift-right.

; CHECK-LABEL: zext_i8:
; CHECK: LDI     #255, R2
; CHECK: AND
define i64 @zext_i8(i8 %x) {
entry:
  %y = zext i8 %x to i64
  ret i64 %y
}

; CHECK-LABEL: sext_i8:
; CHECK: LDI     #56, [[SH:R[0-9]+]]
; CHECK: SLL     [[SH]], R1
; CHECK-NEXT: SRA     [[SH]], R1
define i64 @sext_i8(i8 %x) {
entry:
  %y = sext i8 %x to i64
  ret i64 %y
}

; CHECK-LABEL: zext_i16:
; CHECK: LDI     #255, R2
; CHECK-NEXT: LDI.S1  #255, R2
; CHECK: AND
define i64 @zext_i16(i16 %x) {
entry:
  %y = zext i16 %x to i64
  ret i64 %y
}

; CHECK-LABEL: sext_i16:
; CHECK: LDI     #48, [[SH:R[0-9]+]]
; CHECK: SLL     [[SH]], R1
; CHECK-NEXT: SRA     [[SH]], R1
define i64 @sext_i16(i16 %x) {
entry:
  %y = sext i16 %x to i64
  ret i64 %y
}