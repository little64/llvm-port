# RUN: not llvm-mc -triple=little64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

# LS register-indirect offsets are limited to {0,2,4,6} bytes.

LOAD [R3+8], R1

# CHECK: error: invalid operand for instruction
# CHECK: LOAD [R3+8], R1