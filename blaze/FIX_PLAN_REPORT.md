# Blaze Compiler Fix Plan Report

## Executive Summary
After thorough reconnaissance of the Blaze compiler codebase, I've identified several critical issues that need immediate attention. The fixes are prioritized by severity and impact on stability.

## Critical Issues Found

### 1. **Buffer Overflow in emit_byte Functions** (CRITICAL)
**Location**: `include/blaze_internals.h:453-457`
```c
static inline void emit_byte(CodeBuffer* buf, uint8_t byte) {
    if (buf->position < buf->capacity) {
        buf->code[buf->position++] = byte;
    }
}
```
**Problem**: No error reporting when buffer is full - silently drops bytes!
**Impact**: Memory corruption, unpredictable crashes
**Fix Required**: Add bounds checking and error propagation

### 2. **O2/O3 Optimization Crashes** (HIGH)
**Symptoms**: Segmentation fault on startup with -O2 or -O3
**Root Cause**: Missing volatile qualifiers and register clobbers in inline assembly
**Evidence**: Works with -O0/-O1, crashes with -O2/-O3
**Fix Required**: Add proper volatile qualifiers to critical data structures

### 3. **Function Parameter Handling Missing** (HIGH)
**Location**: `src/codegen/codegen_func.c:219`
```c
// TODO: Handle parameters from right_idx
// For now assume no parameters
```
**Impact**: Functions cannot accept parameters
**Fix Required**: Implement full parameter passing

### 4. **Fixed Size Limitations** (MEDIUM)
**Issues Found**:
- Variable table: 256 max (`codegen_vars.c:33`)
- Function table: 256 max (`codegen_func.c:24`)
- No overflow checking
**Fix Required**: Dynamic allocation or proper error handling

### 5. **Parser String Pool Overflow** (MEDIUM)
**Location**: `parser_core.c:15-22`
```c
static inline bool safe_string_pool_write(Parser* p, char c) {
    if (p->string_pos >= 4095) { // Hard limit
        p->has_error = true;
        return false;
    }
    p->string_pool[p->string_pos++] = c;
    return true;
}
```
**Problem**: Fixed 4KB string pool with no dynamic growth

## Detailed Fix Plan

### Phase 1: Critical Stability Fixes (Immediate)

#### Fix 1.1: Buffer Overflow Protection
```c
// Replace current emit_byte with:
static inline bool emit_byte_safe(CodeBuffer* buf, uint8_t byte) {
    if (buf->position >= buf->capacity) {
        buf->error = true;
        return false;
    }
    buf->code[buf->position++] = byte;
    return true;
}

// Add to CodeBuffer struct:
bool error;  // Track emission errors
```

#### Fix 1.2: O2/O3 Crash Resolution
- Add volatile to all global buffers
- Fix inline assembly constraints
- Add memory barriers where needed

### Phase 2: High Priority Features (Week 1)

#### Fix 2.1: Function Parameters
```c
// In generate_func_call():
if (node->data.func_call.right_idx != 0xFFFF) {
    // Generate parameter evaluation
    generate_expression(buf, nodes, param_idx, symbols, string_pool);
    // Push result based on calling convention
    emit_push_reg(buf, RAX);  // Simple stack-based passing
}
```

#### Fix 2.2: Dynamic Table Management
- Replace fixed arrays with growable structures
- Add proper bounds checking
- Implement reallocation strategy

### Phase 3: Medium Priority (Week 2)

#### Fix 3.1: Parser Improvements
- Dynamic string pool allocation
- Better error recovery
- Bounds checking on all operations

#### Fix 3.2: Memory Safety Audit
- Add NULL checks throughout
- Validate all array accesses
- Implement defensive programming patterns

## Implementation Priority

1. **IMMEDIATE** (Today):
   - Fix emit_byte buffer overflow
   - Add error flags to CodeBuffer

2. **HIGH** (This Week):
   - Fix O2/O3 crashes
   - Implement function parameters
   - Add table overflow checks

3. **MEDIUM** (Next Week):
   - Dynamic memory allocation
   - Comprehensive error handling
   - Parser robustness improvements

## Testing Strategy

1. **Regression Tests**:
   - All current working features
   - Edge cases for each fix

2. **Stress Tests**:
   - Large programs (>1000 lines)
   - Deep nesting
   - Many variables/functions

3. **Optimization Tests**:
   - Test with -O0, -O1, -O2, -O3
   - Verify identical behavior

## Risk Assessment

- **Highest Risk**: Buffer overflows causing memory corruption
- **Medium Risk**: Fixed size limits causing unexpected failures
- **Low Risk**: Missing features (parameters) - fails predictably

## Recommendations

1. Start with emit_byte fix - prevents crashes
2. Fix O2/O3 issues for better performance
3. Implement parameters for full functionality
4. Improve error handling throughout

## Timeline Estimate

- Phase 1: 1-2 days
- Phase 2: 3-5 days  
- Phase 3: 5-7 days
- Testing: 2-3 days

**Total: ~2 weeks for comprehensive fixes**