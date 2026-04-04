# RUN: llvm-mc -triple=little64 -filetype=obj %s -o - | \
# RUN:   llvm-objdump -d --triple=little64 - | FileCheck %s

# Mnemonics and register names are case-insensitive in the parser.

MoVe R1, pC
LdI64 #42, r2
jUmP.z @+1

# CHECK: MOVE    R1, PC
# CHECK-NEXT: LDI     #42, R2
# CHECK-NEXT: JUMP.Z  @+1