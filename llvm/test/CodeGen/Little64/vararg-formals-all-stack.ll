; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Variadic callees use the all-stack calling convention. Fixed formal args
; must be loaded from stack slots, not from incoming argument registers.

; CHECK-LABEL: vararg_fixed_sum:
; CHECK: LOAD    [R13], R1
; CHECK: LDI     #8, R12
; CHECK: ADD     R13, R12
; CHECK: LOAD    [R12], R2
; CHECK: ADD     R2, R1
define i64 @vararg_fixed_sum(i64 %a, i64 %b, ...) {
entry:
  %s = add i64 %a, %b
  ret i64 %s
}

; CHECK-LABEL: vararg_fixed_passthrough:
; CHECK: LOAD    [R13], R1
define i64 @vararg_fixed_passthrough(i64 %a, ...) {
entry:
  ret i64 %a
}

; CHECK-LABEL: vararg_fixed_third:
; CHECK: LDI     #16, R12
; CHECK: ADD     R13, R12
; CHECK: LOAD    [R12], R1
define i64 @vararg_fixed_third(i64 %a, i64 %b, i64 %c, ...) {
entry:
  ret i64 %c
}