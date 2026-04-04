; RUN: llc -mtriple=little64 -O2 < %s | FileCheck %s
;
; Keep i64 zero materialization anchored to the architectural zero register.
; This prevents mutable temporary webs from becoming the source of values that
; semantically must stay constant zero.

declare i64 @callee5(i64, i64, i64, i64, i64)

; CHECK-LABEL: ret_zero:
; CHECK: MOVE    R0, R1
; CHECK-NOT: LDI     #0, R1

define i64 @ret_zero() {
entry:
  ret i64 0
}

; CHECK-LABEL: pass_zero_arg:
; CHECK: MOVE    R0, R10
; CHECK: .quad callee5

define i64 @pass_zero_arg(i64 %x) {
entry:
  %r = call i64 @callee5(i64 0, i64 %x, i64 1, i64 2, i64 3)
  ret i64 %r
}
