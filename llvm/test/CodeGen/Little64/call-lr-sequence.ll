; RUN: llc -mtriple=little64 -O1 < %s | FileCheck %s
;
; Verify direct-call lowering computes LR from the instruction right before
; the final branch-to-target register.
;
; CHECK-LABEL: do_call:
; CHECK: JUMP    @+4
; CHECK: .quad ext_target
; CHECK: LOAD    @-5, R1
; CHECK-NEXT: MOVE    R15, R14+2
; CHECK-NEXT: MOVE    R1, R15

declare i64 @ext_target(i64)

define i64 @do_call(i64 %x) {
entry:
  %v = call i64 @ext_target(i64 %x)
  ret i64 %v
}
