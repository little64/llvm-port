; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Inspired by backend legalization tests in other targets: verify expanded
; lowering for bswap/rotates without dedicated ISA ops.

; CHECK-LABEL: bswap64:
; CHECK: SLLI
; CHECK: SRLI
; CHECK: OR

define i64 @bswap64(i64 %x) {
entry:
  %r = call i64 @llvm.bswap.i64(i64 %x)
  ret i64 %r
}

; CHECK-LABEL: rotl64:
; CHECK: AND
; CHECK: SLL
; CHECK: SRL
; CHECK: OR

define i64 @rotl64(i64 %x, i64 %n) {
entry:
  %r = call i64 @llvm.fshl.i64(i64 %x, i64 %x, i64 %n)
  ret i64 %r
}

; CHECK-LABEL: rotr64:
; CHECK: AND
; CHECK: SRL
; CHECK: SLL
; CHECK: OR

define i64 @rotr64(i64 %x, i64 %n) {
entry:
  %r = call i64 @llvm.fshr.i64(i64 %x, i64 %x, i64 %n)
  ret i64 %r
}

declare i64 @llvm.bswap.i64(i64)
declare i64 @llvm.fshl.i64(i64, i64, i64)
declare i64 @llvm.fshr.i64(i64, i64, i64)
