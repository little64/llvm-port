# RUN: not llvm-mc -triple=little64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR

# CALL pseudo currently accepts register operands only in parser mode.

CALL ext_target

# ERR: error: invalid operand for instruction
