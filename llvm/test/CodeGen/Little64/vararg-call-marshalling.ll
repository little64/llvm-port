; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Verify vararg caller marshalling order, stack staging, and call sequence.

declare i64 @sink_i64(i64, ...)

; CHECK-LABEL: call_vararg_i32:
; CHECK: LDI     #24, R12
; CHECK: SUB     R12, R13
; CHECK: STORE   [R1], R2
; CHECK: STORE   [R2], R9
; CHECK: STORE   [R1], R10
; CHECK: .quad   sink_i64
; CHECK: MOVE    R15, R14+2
define i64 @call_vararg_i32(i32 %a, i32 %b) {
entry:
  %r = call i64 (i64, ...) @sink_i64(i64 1, i32 %a, i32 %b)
  ret i64 %r
}

; CHECK-LABEL: call_vararg_smallints:
; CHECK: LDI     #32, R12
; CHECK: SUB     R12, R13
; CHECK: STORE   [R1], R2
; CHECK: STORE   [R2], R8
; CHECK: STORE   [R2], R9
; CHECK: STORE   [R1], R10
; CHECK: .quad   sink_i64
define i64 @call_vararg_smallints(i8 %a, i16 %b, i32 %c) {
entry:
  %r = call i64 (i64, ...) @sink_i64(i64 2, i8 %a, i16 %b, i32 %c)
  ret i64 %r
}

; CHECK-LABEL: call_vararg_many:
; CHECK: LDI     #72, R12
; CHECK: SUB     R12, R13
; CHECK: LDI     #123, R2
; CHECK: LDI     #99, R2
; CHECK: STORE   [R2], R6
; CHECK: STORE   [R2], R7
; CHECK: STORE   [R2], R8
; CHECK: STORE   [R2], R9
; CHECK: STORE   [R1], R10
; CHECK: .quad   sink_i64
define i64 @call_vararg_many(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f) {
entry:
  %r = call i64 (i64, ...) @sink_i64(i64 7, i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f, i64 99, i64 123)
  ret i64 %r
}