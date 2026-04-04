; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; LowerBR_CC should materialize integer zero as R0 instead of creating a
; separate constant register for compare-against-zero branches.

; CHECK-LABEL: cmp_zero_branch:
; CHECK: TEST    R0,
; CHECK: JUMP.Z
; CHECK: JUMP

define i64 @cmp_zero_branch(i64 %x, i64 %a, i64 %b) {
entry:
  %cmp = icmp eq i64 %x, 0
  br i1 %cmp, label %is_zero, label %not_zero

is_zero:
  ret i64 %a

not_zero:
  ret i64 %b
}
