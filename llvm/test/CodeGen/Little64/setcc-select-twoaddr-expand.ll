; RUN: llc -mtriple=little64 -O2 -stop-after=little64-expand-pseudos %s -o - | FileCheck %s
;
; Regression: SETCC_SELECT is expanded in a late pre-emit pass. CMOV keeps the
; destination as the false value, so expansion must ensure the CMOV false
; operand is tied to destination (and materialized beforehand when needed).

; CHECK-LABEL: name: select_bug
; CHECK: TEST
; CHECK: $[[DST:r[0-9]+]] = CMOV_{{[A-Z]+}} $r{{[0-9]+}}, $[[DST]], implicit $flags

define i64 @select_bug(i64 %a, i64 %b, i64 %c) {
entry:
  %cmp = icmp sgt i64 %a, %b
  %sub = sub i64 %a, %b
  %sel = select i1 %cmp, i64 %sub, i64 %c
  %res = add i64 %sel, %a
  ret i64 %res
}
