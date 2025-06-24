# Blaze Compiler Fix Plan - High Priority Issues

## Executive Summary
After analyzing the Blaze compiler codebase, I've identified several critical issues that need immediate attention. These issues range from missing bounds checking to incomplete implementations and optimization crashes.

## 1. **CRITICAL: Missing Bounds Checking in emit_byte Functions**

### Issue
The `emit_byte`, `emit_dword`, and `emit_qword` functions appear to write directly to the code buffer without checking if there's enough space. This could lead to buffer overflows.

### Location
- Functions are declared as `extern` in multiple files but no implementation with bounds checking was found
- Used extensively in: `src/codegen/*.c`

### Fix Required
```c
// Add to codegen_x64.c or create codegen_emit.c
void emit_byte(CodeBuffer* buf, uint8_t byte) {
    if (buf->position >= buf->capacity) {
        // Handle buffer overflow - either grow buffer or error
        print_str("[ERROR] Code buffer overflow at position ");
        print_num(buf->position);
        print_str("\n");
        return;
    }
    buf->code[buf->position++] = byte;
}

void emit_dword(CodeBuffer* buf, uint32_t value) {
    if (buf->position + 4 > buf->capacity) {
        print_str("[ERROR] Code buffer overflow in emit_dword\n");
        return;
    }
    *(uint32_t*)(buf->code + buf->position) = value;
    buf->position += 4;
}

void emit_qword(CodeBuffer* buf, uint64_t value) {
    if (buf->position + 8 > buf->capacity) {
        print_str("[ERROR] Code buffer overflow in emit_qword\n");
        return;
    }
    *(uint64_t*)(buf->code + buf->position) = value;
    buf->position += 8;
}
```

## 2. **TODO: Function Parameter Handling Missing**

### Issue
Function calls don't handle parameters - hardcoded comment found.

### Location
- `src/codegen/codegen_func.c:219` - "// TODO: Handle parameters from right_idx"

### Fix Required
- Implement parameter passing following System V AMD64 ABI
- First 6 integer args: RDI, RSI, RDX, RCX, R8, R9
- First 8 float args: XMM0-XMM7
- Additional args on stack

## 3. **Type 243 AST Corruption**

### Issue
The compiler checks for type 243 errors but the root cause is unknown. This appears to be memory corruption.

### Location
- `src/blaze_compiler_main.c:191-207` - Type 243 detection code

### Potential Causes
1. Buffer overflow in parser (string pool or node allocation)
2. Uninitialized memory in AST nodes
3. Stack corruption from recursive parsing

### Fix Required
- Add bounds checking to all parser allocation functions
- Initialize all AST node fields to zero
- Add guard values around critical structures

## 4. **Optimization Crashes (-O2, -O3)**

### Issue
According to Makefile comments, -O2 and -O3 optimizations cause crashes on startup.

### Location
- `Makefile:12-14` - Comments about optimization status

### Root Cause
Missing volatile/clobber specifications in inline assembly or syscalls, causing GCC to optimize incorrectly.

### Fix Required
- Audit all syscall sites for proper clobber lists
- Add memory barriers where needed
- Consider using `__attribute__((optimize("O0")))` for critical startup code

## 5. **Parser Buffer Overflow Risks**

### Issue
Multiple places in parser have potential buffer overflows.

### Locations
- `src/parser/parser_core.c:107` - String pool bounds check but continues on error
- `src/parser/parser_core.c:140` - String literal processing with insufficient bounds checking
- Node allocation at capacity continues with error flag but may corrupt memory

### Fix Required
```c
// Improve error handling - don't continue on buffer overflow
static uint32_t store_string(Parser* p, Token* tok) {
    if (p->string_pos + tok->len + 1 > 4096) {
        p->has_error = true;
        print_str("[PARSER] FATAL: String pool overflow\n");
        // Don't return 0 offset - that's valid!
        return 0xFFFFFFFF; // Invalid offset marker
    }
    // ... rest of function
}
```

## 6. **Variable Table Fixed Size Limitations**

### Issue
Variable table has hardcoded limit of 256 variables with no overflow protection.

### Location
- `src/codegen/codegen_vars.c:33` - `#define MAX_VARS 256`
- `src/codegen/codegen_vars.c:93` - Returns NULL on overflow but callers don't check

### Fix Required
- Add NULL checks in all callers
- Consider dynamic allocation or larger fixed size
- Report clear error message when limit exceeded

## 7. **Function Table Fixed Size Limitations**

### Issue
Function table limited to 256 entries, fixup list also limited to 256.

### Location
- `src/codegen/codegen_func.c:15` - Static array of 256 functions
- `src/codegen/codegen_func.c:24` - Static array of 256 fixups

### Fix Required
- Add overflow checks and error reporting
- Consider linked list or dynamic allocation for scalability

## 8. **Missing Error Propagation**

### Issue
Many functions set error flags but continue processing, potentially causing cascading failures.

### Example Locations
- Parser continues after allocation failures
- Code generation continues after variable lookup failures
- No consistent error return convention

### Fix Required
- Establish consistent error handling convention
- Add early returns on critical errors
- Propagate errors up the call stack

## Implementation Priority

1. **Immediate (Prevents Crashes)**
   - Add bounds checking to emit_byte/dword/qword functions
   - Fix parser buffer overflow handling
   - Add NULL checks for variable/function lookups

2. **High (Core Functionality)**
   - Implement function parameter handling
   - Fix optimization crashes with proper clobber lists
   - Improve error propagation

3. **Medium (Robustness)**
   - Investigate type 243 corruption root cause
   - Increase or dynamicize table sizes
   - Add memory guards and debugging aids

## Testing Strategy

1. Create stress tests for:
   - Large number of variables (>256)
   - Large number of functions (>256)
   - Deep nesting and recursion
   - Long strings and identifiers

2. Run with valgrind/AddressSanitizer to catch memory issues

3. Test each optimization level systematically

4. Use Sentry integration to track errors in production

## Estimated Timeline

- Phase 1 (Critical fixes): 2-3 days
- Phase 2 (Core functionality): 3-4 days  
- Phase 3 (Robustness): 2-3 days
- Testing and validation: 2-3 days

Total: ~2 weeks for comprehensive fixes