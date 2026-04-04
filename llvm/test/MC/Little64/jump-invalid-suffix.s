# RUN: not llvm-mc -triple=little64 %s -o /dev/null 2>&1 | FileCheck %s

# Unsupported condition suffix must be rejected.

jump.ne @+2

# CHECK: error: unknown instruction mnemonic
# CHECK: jump.ne @+2