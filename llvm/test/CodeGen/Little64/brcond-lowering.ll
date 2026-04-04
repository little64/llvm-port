; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Verify BRCOND is lowered through target custom lowering to BRCC expansion
; (TEST + conditional jump + unconditional jump), rather than legalizer churn.

; CHECK-LABEL: brcond_i1_arg:
; CHECK: TEST
; CHECK: JUMP.Z
; CHECK: JUMP
define i64 @brcond_i1_arg(i1 %c, i64 %a, i64 %b) {
entry:
  br i1 %c, label %t, label %f

t:
  ret i64 %a

f:
  ret i64 %b
}

; CHECK-LABEL: brcond_i1_from_trunc:
; CHECK: TEST
; CHECK: JUMP.Z
; CHECK: JUMP
define i64 @brcond_i1_from_trunc(i64 %x, i64 %a, i64 %b) {
entry:
  %t = trunc i64 %x to i1
  br i1 %t, label %yes, label %no

yes:
  ret i64 %a

no:
  ret i64 %b
}

; CHECK-LABEL: brcc_path_intcmp:
; CHECK: TEST
; CHECK: JUMP.Z
define i64 @brcc_path_intcmp(i64 %x, i64 %a, i64 %b) {
entry:
  %cmp = icmp ne i64 %x, 0
  br i1 %cmp, label %yes, label %no

yes:
  ret i64 %a

no:
  ret i64 %b
}
