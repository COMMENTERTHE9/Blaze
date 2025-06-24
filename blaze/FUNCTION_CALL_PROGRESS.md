# Function Call Progress

## Fixed Issues

1. **Parser Field Overlap**: Fixed by storing function name in upper 16 bits of temporal_offset instead of binary.right_idx which was being overwritten by statement chaining.

2. **Symbol Builder**: Updated to read function name from the correct field.

3. **Code Generation**: Function is now correctly identified and called with proper jump offset calculation.

## Current Status

The function is being called successfully:
- Function definition is generated at offset 24
- Function call generates correct jump offset (-402)
- Function executes and prints "5"

## Remaining Issue

The function doesn't return properly to continue main execution. The expected output is "1\n5\n2" but we only get "5".

This appears to be because:
1. Main code that should print "1" is not being executed
2. After the function prints "5", it doesn't return to print "2"

The issue seems to be with the order of code generation or the initial jump that should skip over function definitions.

## Test Files

- `test_one_func.blaze`: Original test with declare block
- `test_debug.blaze`: Simplified version without declare block

Both show similar issues with execution order.