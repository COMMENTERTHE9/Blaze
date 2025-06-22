# AST Node Union Overlap Analysis

## Union Memory Layout

The AST node union has multiple structs that overlap in memory. Here's the layout:

```c
union {
    // ident struct:
    struct {
        uint32_t name_offset;  // bytes 0-3
        uint16_t name_len;     // bytes 4-5
    } ident;
    
    // binary struct:
    struct {
        TokenType op;          // bytes 0-3 (assuming TokenType is 32-bit)
        uint16_t left_idx;     // bytes 4-5
        uint16_t right_idx;    // bytes 6-7
    } binary;
    
    // timing struct:
    struct {
        TokenType timing_op;   // bytes 0-3
        uint16_t expr_idx;     // bytes 4-5
        int32_t temporal_offset; // bytes 6-9
    } timing;
}
```

## The Problem

When you store a variable definition (NODE_VAR_DEF), you're using:
- `ident.name_offset` (bytes 0-3) for the variable name offset
- `ident.name_len` (bytes 4-5) for the variable name length

But you're also using:
- `timing.temporal_offset` (bytes 6-9) to store the variable type
- `binary.left_idx` (bytes 4-5) to store the init expression

**This creates a conflict!** The `binary.left_idx` at bytes 4-5 overlaps with `ident.name_len` at bytes 4-5.

## What's Happening

1. You store the variable name: `ident.name_offset` and `ident.name_len`
2. You store the type: `timing.temporal_offset` (this is OK, different bytes)
3. You store the init expression: `binary.left_idx` **This overwrites `ident.name_len`!**

This explains:
- "test" becoming length 3: The init expression node index might be 3
- "zero" becoming length 25: The init expression node index might be 25

## Solutions

### Solution 1: Use Different Fields
Store the init expression in a field that doesn't overlap with `ident`:
```c
// Use timing.expr_idx (bytes 4-5) which ALSO overlaps! Don't use this.
// Use binary.right_idx (bytes 6-7) which overlaps with timing.temporal_offset!
```

### Solution 2: Create a Dedicated Variable Struct
Add a new struct to the union specifically for variable definitions:
```c
// Variable definition
struct {
    uint32_t name_offset;      // bytes 0-3
    uint16_t name_len;         // bytes 4-5
    uint8_t var_type;          // byte 6 (0=var, 1=const, 2=int, etc.)
    uint8_t reserved;          // byte 7 (padding)
    uint16_t init_expr_idx;    // bytes 8-9
} var_def;
```

### Solution 3: Store Name and Init Separately
Instead of storing everything in one node, use a pattern where:
- The VAR_DEF node stores the variable type and points to a name node
- The name is stored in a separate IDENTIFIER node
- The init expression is stored in another separate node

### Solution 4: Pack Data Carefully
Use fields that don't overlap:
```c
// Current usage:
ident.name_offset = offset;     // bytes 0-3 ✓
ident.name_len = len;          // bytes 4-5 ✓
timing.temporal_offset = type;  // bytes 6-9 ✓
binary.left_idx = init;        // bytes 4-5 ✗ CONFLICTS with name_len!

// Fixed usage:
ident.name_offset = offset;     // bytes 0-3 ✓
ident.name_len = len;          // bytes 4-5 ✓
binary.right_idx = init;       // bytes 6-7 ✗ CONFLICTS with temporal_offset!

// Better: Store type and init together
ident.name_offset = offset;     // bytes 0-3
ident.name_len = len;          // bytes 4-5
// Pack type (8 bits) and init (16 bits) into temporal_offset (32 bits)
timing.temporal_offset = (type << 16) | init;  // bytes 6-9
```

## Recommended Fix

The cleanest solution is to pack the variable type and init expression into the `temporal_offset` field:

```c
// Store name
p->nodes[var_node].data.ident.name_offset = name_offset;
p->nodes[var_node].data.ident.name_len = name_len;

// Pack type and init into temporal_offset
// Upper 16 bits: variable type
// Lower 16 bits: init expression index
uint32_t packed = ((uint32_t)var_type << 16) | (init_expr & 0xFFFF);
p->nodes[var_node].data.timing.temporal_offset = packed;

// To read back:
uint8_t var_type = (node->data.timing.temporal_offset >> 16) & 0xFF;
uint16_t init_expr = node->data.timing.temporal_offset & 0xFFFF;
```

This avoids all overlaps and keeps the data organized.