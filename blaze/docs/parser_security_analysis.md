# Parser Core Security Vulnerability Analysis

## Critical Vulnerabilities Found

### 1. **Buffer Overflow in store_string() - Lines 91-107**
**CRITICAL: Missing string pool overflow check for null terminator**
```c
// Line 104: This write can overflow!
p->string_pool[p->string_pos++] = '\0';
```
**Issue**: The function checks if `string_pos >= 4096` inside the loop (line 96), but after the loop it unconditionally writes a null terminator without checking if there's space.

**Exploit**: A token with length exactly 4095 will pass all checks but overflow when null terminator is added.

### 2. **Buffer Overflow in store_string_literal() - Lines 110-141**
**CRITICAL: No overflow checks at all**
```c
// Lines 125-133: No bounds checking!
p->string_pool[p->string_pos++] = '\n'; // etc.
p->string_pool[p->string_pos++] = c;
```
**Issue**: The function never checks if `string_pos` exceeds the string pool size. It blindly writes characters.

**Exploit**: Any string literal can overflow the string pool.

### 3. **Buffer Overflow in parse_var_def() - Lines 537-546**
**CRITICAL: Missing overflow check in loop**
```c
// Line 539: No overflow check!
p->string_pool[p->string_pos++] = c;
```
**Issue**: The loop copies characters without checking string pool bounds.

**Exploit**: Long variable names overflow the string pool.

### 4. **Buffer Overflow in Function Call Parsing - Lines 1186-1193**
**MODERATE: Inconsistent overflow checking**
```c
// Line 1187-1190: Checks inside loop
if (p->string_pos >= 4096) {
    print_str("  ERROR: string pool overflow\n");
    return 0;
}
// Line 1193: But no check for null terminator!
p->string_pool[p->string_pos++] = '\0';
```
**Issue**: Same as issue #1 - null terminator can overflow.

### 5. **str_equals() Vulnerability - Lines 6-11**
**CRITICAL: No bounds checking on source buffers**
```c
static inline bool str_equals(const char* a, const char* b, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        if (a[i] != b[i]) return false;  // Can read past buffer end!
    }
    return true;
}
```
**Issue**: If `len` is larger than either string buffer, this will read out of bounds.

**Exploit**: Malformed token with incorrect length can cause out-of-bounds reads.

### 6. **Source Buffer Access Without Bounds Checking**
Multiple locations access `p->source[tok->start + i]` without verifying that `tok->start + i` is within source bounds:
- Line 100: `p->source[tok->start + i]`
- Line 119: `p->source[tok->start + i]`
- Line 146: `p->source[num_tok->start + i]`
- Line 468: `p->source[var_tok->start + var_tok->len - 1]`
- Line 538: `p->source[name_start + i]`
- Line 1172: `p->source[name_tok->start + i]`

**Issue**: If tokens have invalid start/len values, these accesses can read out of bounds.

### 7. **Peek Functions Are Safe - Lines 35-43**
**GOOD**: The peek2 and peek3 functions properly check bounds:
```c
if (p->current + 1 >= p->count) return NULL;  // Line 36
if (p->current + 2 >= p->count) return NULL;  // Line 41
```

### 8. **alloc_node() Is Safe - Lines 64-88**
**GOOD**: Properly checks node pool capacity:
```c
if (p->node_count >= p->node_capacity) {  // Line 65
    p->has_error = true;
    return 0;
}
```

### 9. **Union Clearing Is Safe - Lines 82-85**
**GOOD**: Using `sizeof(node->data)` is safe because it's a compile-time constant.

## Summary of Required Fixes

### High Priority Fixes:

1. **Fix store_string()**: Add overflow check before null terminator:
```c
if (p->string_pos >= 4096) {
    p->has_error = true;
    return 0;
}
p->string_pool[p->string_pos++] = '\0';
```

2. **Fix store_string_literal()**: Add overflow checks:
```c
// Before each write:
if (p->string_pos >= 4096) {
    p->has_error = true;
    return 0;
}
```

3. **Fix str_equals()**: Add length validation or use safer comparison

4. **Add source buffer bounds checking**: Validate `tok->start + tok->len` doesn't exceed source length

### Recommended Security Improvements:

1. Define constants for buffer sizes:
```c
#define STRING_POOL_SIZE 4096
#define MAX_TOKEN_LENGTH 1024
```

2. Create safe string copy function:
```c
static bool safe_copy_to_pool(Parser* p, const char* src, uint32_t len) {
    if (p->string_pos + len + 1 > STRING_POOL_SIZE) {
        p->has_error = true;
        return false;
    }
    // Copy with bounds checking
    for (uint32_t i = 0; i < len; i++) {
        p->string_pool[p->string_pos++] = src[i];
    }
    p->string_pool[p->string_pos++] = '\0';
    return true;
}
```

3. Validate all token bounds before use:
```c
static bool validate_token(Parser* p, Token* tok) {
    if (!tok) return false;
    if (tok->start + tok->len > p->source_len) return false;
    if (tok->len > MAX_TOKEN_LENGTH) return false;
    return true;
}
```

These vulnerabilities could lead to buffer overflows, potentially allowing arbitrary code execution or crashes.