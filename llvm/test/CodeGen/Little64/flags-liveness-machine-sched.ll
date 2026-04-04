; RUN: llc -mtriple=little64 -O2 -stop-after=machine-scheduler %s -o - | FileCheck %s
;
; Verify condition-code dependencies are explicit in Machine IR so scheduling
; cannot move flag-clobbering instructions between compare and consumers.

; CHECK-LABEL: name: branch_cmp
; CHECK: BRCC
; CHECK-SAME: %bb.{{[0-9]+}}, 1, %{{[0-9]+}}, %{{[0-9]+}}
; CHECK: JMP

define i64 @branch_cmp(i64 %a, i64 %b) {
entry:
  %cmp = icmp eq i64 %a, %b
  br i1 %cmp, label %t, label %f

t:
  ret i64 1

f:
  ret i64 0
}

; CHECK-LABEL: name: select_cmp
; CHECK: SETCC_SELECT

define i64 @select_cmp(i64 %x, i64 %y, i64 %a, i64 %b) {
entry:
  %cmp = icmp slt i64 %x, %y
  %r = select i1 %cmp, i64 %a, i64 %b
  ret i64 %r
}
