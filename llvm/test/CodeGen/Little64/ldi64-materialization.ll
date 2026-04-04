; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Cover LDI64 expansion paths:
; 1) sign-extendable i32 immediates -> byte-wise LDI sequence
; 2) non-sign-extendable 64-bit immediates -> literal slot path

; CHECK-LABEL: const_small32:
; CHECK: LDI
; CHECK: LDI.S1
; CHECK: LDI.S2
; CHECK: LDI.S3
; CHECK-NOT: .quad

define i64 @const_small32() {
entry:
  ret i64 305419896 ; 0x12345678
}

; CHECK-LABEL: const_negative32:
; CHECK: LDI
; CHECK: LDI.S1
; CHECK: LDI.S2
; CHECK: LDI.S3

define i64 @const_negative32() {
entry:
  ret i64 -1
}

; CHECK-LABEL: const_large64:
; CHECK: JUMP    @+4
; CHECK-NEXT: .quad 1234605616436508552
; CHECK-NEXT: LOAD    @-5, R1

define i64 @const_large64() {
entry:
  ret i64 1234605616436508552 ; 0x1122334455667788
}
