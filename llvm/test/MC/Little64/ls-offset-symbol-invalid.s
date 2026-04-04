# RUN: not llvm-mc -triple=little64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

# mem_ri requires an encodable constant offset, not a symbolic expression.

LOAD [R3+target], R1

target:
  STOP

# CHECK: error: invalid operand for instruction
# CHECK: LOAD [R3+target], R1