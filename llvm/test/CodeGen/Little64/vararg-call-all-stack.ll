; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Vararg calls should use the all-stack outgoing arg convention path.

declare void @var_sink(i64, ...)

; CHECK-LABEL: call_vararg:
; CHECK: LDI     #32, R12
; CHECK: SUB     R12, R13
; CHECK: .quad var_sink
; CHECK: MOVE    R15, R14+2
; CHECK: MOVE    R1, R15
; CHECK: LDI     #32, R12
; CHECK: ADD     R12, R13

define void @call_vararg(i64 %a, i64 %b, i64 %c, i64 %d) {
entry:
  call void (i64, ...) @var_sink(i64 %a, i64 %b, i64 %c, i64 %d)
  ret void
}
