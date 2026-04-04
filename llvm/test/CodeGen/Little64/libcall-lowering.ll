; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Inspired by common backend runtime-helper tests: verify unsupported i64
; arithmetic lowers to the expected compiler-rt/libgcc helpers.

; CHECK-LABEL: mul64:
; CHECK: .quad __muldi3
; CHECK: LOAD    @-5, R1

define i64 @mul64(i64 %a, i64 %b) {
entry:
  %r = mul i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: sdiv64:
; CHECK: .quad __divdi3

define i64 @sdiv64(i64 %a, i64 %b) {
entry:
  %r = sdiv i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: srem64:
; CHECK: .quad __moddi3

define i64 @srem64(i64 %a, i64 %b) {
entry:
  %r = srem i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: udiv64:
; CHECK: .quad __udivdi3

define i64 @udiv64(i64 %a, i64 %b) {
entry:
  %r = udiv i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: urem64:
; CHECK: .quad __umoddi3

define i64 @urem64(i64 %a, i64 %b) {
entry:
  %r = urem i64 %a, %b
  ret i64 %r
}
