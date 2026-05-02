; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Vararg calls should use the all-stack outgoing arg convention path.

declare void @var_sink(i64, ...)

; CHECK-LABEL: call_vararg:
; CHECK: LDI     #40, R12
; CHECK: SUB     R12, R13
; CHECK: MOVE    R13, R1
; CHECK: STORE   [R1], R10
; CHECK: .quad var_sink
; CHECK: MOVE    R15, R14+2
; CHECK: MOVE    R1, R15

define void @call_vararg(i64 %a, i64 %b, i64 %c, i64 %d) {
entry:
  call void (i64, ...) @var_sink(i64 %a, i64 %b, i64 %c, i64 %d)
  ret void
}
