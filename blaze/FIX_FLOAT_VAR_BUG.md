# Float Variable Multiplication Bug Fix

## Issue
When multiplying float variables like `var.f-x-[3.14]` and `var.f-y-[2.0]`, the result is incorrect (4.00 instead of 6.28).

## Root Cause
During float binary operations, when we save the right operand to stack:
1. We evaluate right operand (y=2.0) and get it in XMM0
2. We do `sub rsp, 8` and `movsd [rsp], xmm0` to save it
3. We evaluate left operand (x) which loads from `[rsp+248]`
4. BUT: RSP has changed by 8, so we're actually loading from the wrong address!

The variable loading code uses RSP-relative addressing with a fixed offset calculated at variable creation time. It doesn't account for temporary stack adjustments during expression evaluation.

## Solution
We need to track the stack adjustment during expression evaluation and compensate for it when loading variables. This could be done by:

1. Adding a stack_adjustment field to CodeBuffer to track temporary pushes
2. Modifying generate_var_load_float to add this adjustment to the offset
3. Ensuring we reset the adjustment after each expression

## Workaround
For now, users should use direct float literal multiplication which works correctly:
```blaze
var.v-result-[3.14 * 2.0]  // Works: 6.28
```

Instead of:
```blaze
var.f-x-[3.14]
var.f-y-[2.0]
var.v-z-[x * y]  // Bug: gives 4.00
```