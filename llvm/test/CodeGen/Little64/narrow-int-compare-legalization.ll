; RUN: llc -mtriple=little64 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=little64 -O2 -stop-after=machine-scheduler %s -o - | FileCheck %s --check-prefix=MIR
;
; Ensure non-i64 integer compares are legalized correctly for Little64,
; which only has i64 compare/branch machinery.

; CHECK-LABEL: br_i8_slt:
; CHECK: LDI     #128
; CHECK: LDI     #255
; CHECK: TEST
; CHECK: JUMP.C
; MIR-LABEL: name:            br_i8_slt
; MIR: LDI64 128
; MIR: LDI64 255
; MIR: BRCC %bb.{{[0-9]+}},

define i64 @br_i8_slt(i8 %a, i8 %b, i64 %x, i64 %y) {
entry:
  %c = icmp slt i8 %a, %b
  br i1 %c, label %t, label %f

t:
  ret i64 %x

f:
  ret i64 %y
}

; CHECK-LABEL: br_i8_ult:
; CHECK: LDI     #255
; CHECK: TEST
; CHECK: JUMP.C
; MIR-LABEL: name:            br_i8_ult
; MIR: LDI64 255
; MIR: BRCC %bb.{{[0-9]+}},

define i64 @br_i8_ult(i8 %a, i8 %b, i64 %x, i64 %y) {
entry:
  %c = icmp ult i8 %a, %b
  br i1 %c, label %t, label %f

t:
  ret i64 %x

f:
  ret i64 %y
}

; CHECK-LABEL: br_i16_sge:
; CHECK: LDI.S1  #128
; CHECK: LDI.S1  #255
; CHECK: TEST
; CHECK: JUMP.C
; MIR-LABEL: name:            br_i16_sge
; MIR: LDI64 32768
; MIR: LDI64 65535
; MIR: BRCC %bb.{{[0-9]+}},

define i64 @br_i16_sge(i16 %a, i16 %b, i64 %x, i64 %y) {
entry:
  %c = icmp sge i16 %a, %b
  br i1 %c, label %t, label %f

t:
  ret i64 %x

f:
  ret i64 %y
}

; CHECK-LABEL: br_i16_ule:
; CHECK: LDI.S1  #255
; CHECK: TEST
; CHECK: JUMP.C
; MIR-LABEL: name:            br_i16_ule
; MIR: LDI64 65535
; MIR: BRCC %bb.{{[0-9]+}},

define i64 @br_i16_ule(i16 %a, i16 %b, i64 %x, i64 %y) {
entry:
  %c = icmp ule i16 %a, %b
  br i1 %c, label %t, label %f

t:
  ret i64 %x

f:
  ret i64 %y
}

; CHECK-LABEL: br_i32_slt:
; CHECK: .quad   2147483648
; CHECK: .quad   4294967295
; CHECK: TEST
; CHECK: JUMP.C
; MIR-LABEL: name:            br_i32_slt
; MIR: LDI64 2147483648
; MIR: LDI64 4294967295
; MIR: BRCC %bb.{{[0-9]+}},

define i64 @br_i32_slt(i32 %a, i32 %b, i64 %x, i64 %y) {
entry:
  %c = icmp slt i32 %a, %b
  br i1 %c, label %t, label %f

t:
  ret i64 %x

f:
  ret i64 %y
}

; CHECK-LABEL: br_i32_ugt:
; CHECK: .quad   4294967295
; CHECK: TEST
; CHECK: JUMP.C
; MIR-LABEL: name:            br_i32_ugt
; MIR: LDI64 4294967295
; MIR: BRCC %bb.{{[0-9]+}},

define i64 @br_i32_ugt(i32 %a, i32 %b, i64 %x, i64 %y) {
entry:
  %c = icmp ugt i32 %a, %b
  br i1 %c, label %t, label %f

t:
  ret i64 %x

f:
  ret i64 %y
}
