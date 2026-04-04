# RUN: not llvm-mc -triple=little64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR

# LS register-indirect byte offsets must be one of {0,2,4,6}.
# Odd offsets are invalid and must be rejected.

LOAD [R3+1], R1

# ERR: error: invalid operand for instruction
