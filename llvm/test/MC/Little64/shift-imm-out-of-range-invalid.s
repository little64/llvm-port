# RUN: not llvm-mc -triple=little64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

# GP immediate shifts use a 4-bit shift count field (valid: 0..15).

SRLI #16, R1

# CHECK: error: invalid operand for instruction
# CHECK: SRLI #16, R1
