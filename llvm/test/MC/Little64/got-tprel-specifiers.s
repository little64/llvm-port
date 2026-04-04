; RUN: llvm-mc -triple=little64 -filetype=obj < %s | llvm-readobj --relocations --expand-relocs - | FileCheck %s
;
; Verify that @got and @tprel specifiers in .quad directives produce the
; correct ELF relocation types (R_LITTLE64_GOT64=10, R_LITTLE64_TLS_TPREL64=13).

.text
.global _start

_start:
  JUMP @+4
  .quad external_sym@got
  LOAD @-5, R1

  JUMP @+4
  .quad tls_var@tprel
  LOAD @-5, R2

; CHECK:      Relocations [
; CHECK:        Type: R_LITTLE64_GOT64 (10)
; CHECK:        Symbol: external_sym
; CHECK:        Type: R_LITTLE64_TLS_TPREL64 (13)
; CHECK:        Symbol: tls_var
