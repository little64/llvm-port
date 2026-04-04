# RUN: llvm-mc -triple=little64 -filetype=obj %s -o - | \
# RUN:   llvm-objdump -d --triple=little64 - | FileCheck %s

# Validate parser support for SP/LR aliases and normalized disassembly.

MOVE SP, R1
MOVE LR, R2
MOVE PC, R3
RET

# CHECK: MOVE    R13, R1
# CHECK-NEXT: MOVE    R14, R2
# CHECK-NEXT: MOVE    R15, R3
# CHECK-NEXT: MOVE    R14, PC
