# RUN: llvm-mc -triple=little64 -filetype=obj %s -o - | \
# RUN:   llvm-objdump -d --triple=little64 - | FileCheck %s

# Inspired by assembler syntax-acceptance tests in other targets.

MOVE R1, R2
MOVE R1, R2+0
LOAD [R3], R1
LOAD [R3+2], R1

# CHECK: MOVE    R1, R2
# CHECK-NEXT: MOVE    R1, R2
# CHECK-NEXT: LOAD    [R3], R1
# CHECK-NEXT: LOAD    [R3+2], R1
