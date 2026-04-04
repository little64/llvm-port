; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
; RUN: llc -mtriple=little64 -O2 < %s | FileCheck %s
;
; Verify SELECT lowering through SETCC_SELECT pseudo to TEST + CMOV.* sequence.
; On Little64, conditional register moves reuse the jump opcode encoding but
; are printed as CMOV.CC when the destination is not PC.

; CHECK-LABEL: select_i1_arg:
; CHECK: TEST
; CHECK: CMOV.
define i64 @select_i1_arg(i1 %c, i64 %a, i64 %b) {
entry:
  %result = select i1 %c, i64 %a, i64 %b
  ret i64 %result
}

; CHECK-LABEL: select_setcc_slt:
; CHECK: TEST
; CHECK: CMOV.C
define i64 @select_setcc_slt(i64 %x, i64 %y, i64 %a, i64 %b) {
entry:
  %cmp = icmp slt i64 %x, %y
  %result = select i1 %cmp, i64 %a, i64 %b
  ret i64 %result
}

; CHECK-LABEL: select_setcc_sge:
; CHECK: TEST
; CHECK: CMOV.C
define i64 @select_setcc_sge(i64 %x, i64 %y, i64 %a, i64 %b) {
entry:
  %cmp = icmp sge i64 %x, %y
  %result = select i1 %cmp, i64 %a, i64 %b
  ret i64 %result
}

; CHECK-LABEL: select_setcc_sgt:
; CHECK: TEST
; CHECK: CMOV.C
define i64 @select_setcc_sgt(i64 %x, i64 %y, i64 %a, i64 %b) {
entry:
  %cmp = icmp sgt i64 %x, %y
  %result = select i1 %cmp, i64 %a, i64 %b
  ret i64 %result
}

; CHECK-LABEL: select_setcc_sle:
; CHECK: TEST
; CHECK: CMOV.C
define i64 @select_setcc_sle(i64 %x, i64 %y, i64 %a, i64 %b) {
entry:
  %cmp = icmp sle i64 %x, %y
  %result = select i1 %cmp, i64 %a, i64 %b
  ret i64 %result
}

; CHECK-LABEL: select_setcc_eq:
; CHECK: TEST
; CHECK: CMOV.Z
define i64 @select_setcc_eq(i64 %x, i64 %y, i64 %a, i64 %b) {
entry:
  %cmp = icmp eq i64 %x, %y
  %result = select i1 %cmp, i64 %a, i64 %b
  ret i64 %result
}

; CHECK-LABEL: select_setcc_ne:
; CHECK: TEST
; CHECK: CMOV.Z
define i64 @select_setcc_ne(i64 %x, i64 %y, i64 %a, i64 %b) {
entry:
  %cmp = icmp ne i64 %x, %y
  %result = select i1 %cmp, i64 %a, i64 %b
  ret i64 %result
}

; CHECK-LABEL: select_setcc_ult:
; CHECK: TEST
; CHECK: CMOV.C
define i64 @select_setcc_ult(i64 %x, i64 %y, i64 %a, i64 %b) {
entry:
  %cmp = icmp ult i64 %x, %y
  %result = select i1 %cmp, i64 %a, i64 %b
  ret i64 %result
}

; CHECK-LABEL: select_setcc_uge:
; CHECK: TEST
; CHECK: CMOV.C
define i64 @select_setcc_uge(i64 %x, i64 %y, i64 %a, i64 %b) {
entry:
  %cmp = icmp uge i64 %x, %y
  %result = select i1 %cmp, i64 %a, i64 %b
  ret i64 %result
}

; CHECK-LABEL: select_setcc_ugt:
; CHECK: TEST
; CHECK: CMOV.C
define i64 @select_setcc_ugt(i64 %x, i64 %y, i64 %a, i64 %b) {
entry:
  %cmp = icmp ugt i64 %x, %y
  %result = select i1 %cmp, i64 %a, i64 %b
  ret i64 %result
}

; CHECK-LABEL: select_setcc_ule:
; CHECK: TEST
; CHECK: CMOV.C
define i64 @select_setcc_ule(i64 %x, i64 %y, i64 %a, i64 %b) {
entry:
  %cmp = icmp ule i64 %x, %y
  %result = select i1 %cmp, i64 %a, i64 %b
  ret i64 %result
}

; CHECK-LABEL: select_nested:
; Test nested selects
; CHECK: TEST
; CHECK: CMOV.
; CHECK: TEST
; CHECK: CMOV.
define i64 @select_nested(i1 %c1, i1 %c2, i64 %a, i64 %b, i64 %c) {
entry:
  %sel1 = select i1 %c1, i64 %a, i64 %b
  %sel2 = select i1 %c2, i64 %sel1, i64 %c
  ret i64 %sel2
}

; CHECK-LABEL: select_with_constant:
; Test select with one constant value
; CHECK: TEST
; CHECK: CMOV.
define i64 @select_with_constant(i64 %x, i64 %y, i64 %a) {
entry:
  %cmp = icmp slt i64 %x, %y
  %result = select i1 %cmp, i64 %a, i64 42
  ret i64 %result
}

; CHECK-LABEL: select_both_constants:
; Test select where both values are constants
; CHECK: LDI
define i64 @select_both_constants(i1 %c) {
entry:
  %result = select i1 %c, i64 10, i64 20
  ret i64 %result
}

; CHECK-LABEL: select_with_load:
; Test select with loaded values
; CHECK: TEST
; CHECK: CMOV.
define i64 @select_with_load(i64 %x, i64 %y, i64* %pa, i64* %pb) {
entry:
  %cmp = icmp ne i64 %x, %y
  %a = load i64, i64* %pa
  %b = load i64, i64* %pb
  %result = select i1 %cmp, i64 %a, i64 %b
  ret i64 %result
}

; CHECK-LABEL: select_in_loop:
; Test select in a loop (ensures proper register handling)
; CHECK: TEST
; CHECK: CMOV.
; CHECK: TEST
; CHECK: JUMP.
define i64 @select_in_loop(i64 %n, i64 %a, i64 %b) {
entry:
  br label %loop

loop:
  %i = phi i64 [0, %entry], [%i_next, %loop]
  %acc = phi i64 [%a, %entry], [%result, %loop]

  %cmp = icmp slt i64 %i, %n
  %result = select i1 %cmp, i64 %acc, i64 %b

  %i_next = add i64 %i, 1
  %done = icmp uge i64 %i_next, %n
  br i1 %done, label %exit, label %loop

exit:
  ret i64 %result
}
