; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Verify calls with more than five i64 args reserve stack call space and spill
; trailing args to the outgoing stack area.

declare i64 @callee7(i64, i64, i64, i64, i64, i64, i64)

; CHECK-LABEL: caller7:
; CHECK: LDI     #16, R12
; CHECK: SUB     R12, R13
; CHECK: STORE   [R2], R1
; CHECK: LDI     #8, R1
; CHECK: ADD     R1, R2
; CHECK: STORE   [R2], R1
; CHECK: .quad callee7
; CHECK: MOVE    R15, R14+2
; CHECK: MOVE    R1, R15
; CHECK: LDI     #16, R12
; CHECK: ADD     R12, R13

define i64 @caller7(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f, i64 %g) {
entry:
  %r = call i64 @callee7(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f, i64 %g)
  ret i64 %r
}
