; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Verify setcc lowered to integer (zext i1->i64) goes through TEST + CMOV
; materialization and condition-specific CMOV opcodes.

; CHECK-LABEL: setcc_eq_i64:
; CHECK: TEST
; CHECK: CMOV.Z

define i64 @setcc_eq_i64(i64 %a, i64 %b) {
entry:
  %c = icmp eq i64 %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; CHECK-LABEL: setcc_sgt_i64:
; CHECK: TEST
; CHECK: CMOV.C

define i64 @setcc_sgt_i64(i64 %a, i64 %b) {
entry:
  %c = icmp sgt i64 %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; CHECK-LABEL: setcc_ult_i64:
; CHECK: TEST
; CHECK: CMOV.C

define i64 @setcc_ult_i64(i64 %a, i64 %b) {
entry:
  %c = icmp ult i64 %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}
