; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Even i32 div/rem are currently legalized through i64 helper libcalls.

; CHECK-LABEL: udiv32:
; CHECK: .quad __udivdi3
define i32 @udiv32(i32 %a, i32 %b) {
entry:
  %q = udiv i32 %a, %b
  ret i32 %q
}

; CHECK-LABEL: urem32:
; CHECK: .quad __umoddi3
define i32 @urem32(i32 %a, i32 %b) {
entry:
  %r = urem i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: sdiv32:
; CHECK: .quad __divdi3
define i32 @sdiv32(i32 %a, i32 %b) {
entry:
  %q = sdiv i32 %a, %b
  ret i32 %q
}

; CHECK-LABEL: srem32:
; CHECK: .quad __moddi3
define i32 @srem32(i32 %a, i32 %b) {
entry:
  %r = srem i32 %a, %b
  ret i32 %r
}