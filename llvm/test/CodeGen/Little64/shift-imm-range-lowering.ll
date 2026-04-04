; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Regression coverage for immediate-shift selection:
; - SLLI/SRLI/SRAI immediate forms have a 4-bit shamt field (0..15).
; - Larger constant shifts must lower to register-shift forms.

; CHECK-NOT: SRLI    #32
; CHECK-NOT: SLLI    #32
; CHECK-NOT: SRAI    #32

; CHECK-LABEL: lshr8:
; CHECK: SRLI    #8, R1
define i64 @lshr8(i64 %x) {
entry:
  %shr = lshr i64 %x, 8
  ret i64 %shr
}

; CHECK-LABEL: lshr32:
; CHECK: LDI     #32, [[SH:R[0-9]+]]
; CHECK: SRL     [[SH]], R1
define i64 @lshr32(i64 %x) {
entry:
  %shr = lshr i64 %x, 32
  ret i64 %shr
}

; CHECK-LABEL: shl_ashr32:
; CHECK: LDI     #32, [[SH:R[0-9]+]]
; CHECK: SLL     [[SH]], R1
; CHECK: SRA     [[SH]], R1
define i64 @shl_ashr32(i64 %x) {
entry:
  %shl = shl i64 %x, 32
  %ashr = ashr i64 %shl, 32
  ret i64 %ashr
}
