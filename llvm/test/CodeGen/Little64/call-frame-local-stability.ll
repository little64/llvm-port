; RUN: llc -mtriple=little64 -O2 < %s | FileCheck %s
;
; Regression for reserved call-frame lowering: a six-argument call in a
; no-frame-pointer function must not emit an extra SUB/ADD R13 bracket around
; the call, or SP-relative locals in the same block will be addressed from the
; wrong slot.

declare i8 @core(i8, i32, ptr, i32, i32, i32)

; CHECK-LABEL: repro:
; CHECK: LDI     #64, R12
; CHECK: SUB     R12, R13
; CHECK: WORD_STORE      [R12], R1
; CHECK: WORD_LOAD       [R12], R6
; CHECK: WORD_LOAD       [R12], R1
; CHECK: STORE   [R13], R1
; CHECK-NEXT: LDI     #8, R10
; CHECK-NEXT: LDI     #170, R9
; CHECK-NEXT: LDI.S1  #1, R9
; CHECK-NEXT: LDI     #36, R12
; CHECK-NEXT: ADD     R13, R12
; CHECK-NEXT: MOVE    R12, R8
; CHECK-NEXT: LDI     #4, R7
; CHECK-NEXT: JUMP    @+4
; CHECK-NEXT: .quad   core
; CHECK: MOVE    R15, R14+2
; CHECK: MOVE    R1, R15
define i8 @repro(ptr %buf, i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f, i64 %g) {
entry:
  %zero = alloca i32, align 8
  %tail = alloca [4 x i8], align 1
  store i32 0, ptr %zero, align 4
  %p = getelementptr inbounds [4 x i8], ptr %tail, i64 0, i64 0
  %s1 = add i64 %a, %b
  %s2 = add i64 %c, %d
  %s3 = add i64 %e, %f
  %s4 = add i64 %s1, %g
  %keep = load volatile i32, ptr %zero, align 4
  %skip = load volatile i32, ptr %zero, align 4
  %r = call i8 @core(i8 8, i32 426, ptr %p, i32 4, i32 %keep, i32 %skip)
  %z = zext i8 %r to i64
  %sum = add i64 %s2, %s3
  %sum2 = add i64 %sum, %s4
  %sum3 = add i64 %sum2, %z
  %ret = trunc i64 %sum3 to i8
  ret i8 %ret
}