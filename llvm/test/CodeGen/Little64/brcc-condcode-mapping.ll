; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Inspired by condition-code mapping tests in other targets.

; CHECK-LABEL: br_slt:
; CHECK: TEST
; CHECK: JUMP.C

define i64 @br_slt(i64 %a, i64 %b) {
entry:
  %cmp = icmp slt i64 %a, %b
  br i1 %cmp, label %t, label %f

t:
  ret i64 1
f:
  ret i64 0
}

; CHECK-LABEL: br_sgt:
; CHECK: TEST
; CHECK: JUMP.C

define i64 @br_sgt(i64 %a, i64 %b) {
entry:
  %cmp = icmp sgt i64 %a, %b
  br i1 %cmp, label %t, label %f

t:
  ret i64 1
f:
  ret i64 0
}

; CHECK-LABEL: br_ult:
; CHECK: TEST
; CHECK: JUMP.C

define i64 @br_ult(i64 %a, i64 %b) {
entry:
  %cmp = icmp ult i64 %a, %b
  br i1 %cmp, label %t, label %f

t:
  ret i64 1
f:
  ret i64 0
}
