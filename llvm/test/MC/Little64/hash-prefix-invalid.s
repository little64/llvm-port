# RUN: not llvm-mc -triple=little64 %s -o /dev/null 2>&1 | FileCheck %s

# Hash prefixes are only accepted as a legacy convenience on LDI64.

MOVE #1, R1

# CHECK: error: invalid operand for instruction
# CHECK: MOVE #1, R1