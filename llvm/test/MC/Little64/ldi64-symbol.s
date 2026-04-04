# RUN: llvm-mc -triple=little64 -filetype=obj %s -o - | \
# RUN:   llvm-objdump -dr --triple=little64 - | FileCheck %s

# Symbol-form LDI64 should use literal slot plus LOAD_PCREL.

LDI64 ext_target, R2

# CHECK: JUMP    @+4
# CHECK: ext_target
# CHECK: LOAD    @-5, R2
