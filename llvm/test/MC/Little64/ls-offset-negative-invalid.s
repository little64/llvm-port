# RUN: not llvm-mc -triple=little64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

# Negative LS register-indirect offsets are not encodable.

LOAD [R3+-2], R1

# CHECK: error: invalid operand for instruction
# CHECK: LOAD [R3+-2], R1