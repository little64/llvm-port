; RUN: llc -mtriple=little64 -verify-machineinstrs < %s | FileCheck %s

define i64 @fp_nonleaf_stack_call(ptr %fn, i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f, i64 %g, i64 %h) #0 {
entry:
  %slot = alloca i64, align 8
  %res = call i64 %fn(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f, i64 %g, i64 %h)
  store i64 %res, ptr %slot, align 8
  %val = load i64, ptr %slot, align 8
  ret i64 %val
}

attributes #0 = { "frame-pointer"="all" }

; CHECK-LABEL: fp_nonleaf_stack_call:
; CHECK: PUSH    R11
; CHECK: MOVE    R13, R11
; CHECK: MOVE    R11, R13
; CHECK-NEXT: LDI     #16, R12
; CHECK-NEXT: ADD     R12, R13
; CHECK: POP     R11