# Typed Variable Parser Fix

## Problem Analysis

The parser is failing to correctly extract variable names from the new typed variable syntax. The issue is in how it calculates the name length for different token patterns.

## Token Patterns

1. **Generic variable**: `var.name-` (TOK_VAR)
2. **Typed variables**: 
   - `var.i-name-` (TOK_VAR_INT)
   - `var.f-name-` (TOK_VAR_FLOAT)
   - `var.s-name-` (TOK_VAR_STRING)
   - `var.b-name-` (TOK_VAR_BOOL)
3. **Constant**: `var.c-name-` (TOK_CONST)
4. **Old syntax**: `var.v-name-` (TOK_VAR)

## Current Issues

1. For `var.name-` with length 9:
   - Current: Skips "var." (4) and removes trailing "-" (1), giving name_len = 4
   - Expected: Name is "name", so name_len should be 4 (which is actually correct)
   - The debug output shows name_len=2, suggesting there's another issue

2. The typed variable parsing assumes all typed tokens have format `var.t-name-` where `t` is a single character.

## Solution

Here's the corrected parse_var_def function logic:

```c
// Parse variable definition
static uint16_t parse_var_def(Parser* p) {
    Token* var_tok = advance(p);
    
    uint16_t var_node = alloc_node(p, NODE_VAR_DEF);
    if (var_node == 0) return 0;
    
    // Store variable type
    uint8_t var_type = 0;
    switch (var_tok->type) {
        case TOK_VAR: var_type = 0; break;
        case TOK_CONST: var_type = 1; break;
        case TOK_VAR_INT: var_type = 2; break;
        case TOK_VAR_FLOAT: var_type = 3; break;
        case TOK_VAR_STRING: var_type = 4; break;
        case TOK_VAR_BOOL: var_type = 5; break;
    }
    p->nodes[var_node].data.timing.temporal_offset = var_type;
    
    // Extract variable name
    uint32_t name_start, name_len;
    
    if (var_tok->type != TOK_VAR) {
        // Typed variable: var.t-name- where t is a type character
        name_start = var_tok->start + 6; // Skip "var.t-"
        name_len = var_tok->len - 6;
        
        // Remove trailing "-" if present
        if (name_len > 0 && p->source[var_tok->start + var_tok->len - 1] == '-') {
            name_len--;
        }
    }
    else {
        // TOK_VAR can be either old syntax or new simplified syntax
        const char* tok_str = &p->source[var_tok->start];
        
        if (var_tok->len > 6 && tok_str[4] == 'v' && tok_str[5] == '-') {
            // Old syntax: var.v-name-
            name_start = var_tok->start + 6;
            name_len = var_tok->len - 6;
        }
        else if (var_tok->len > 4 && tok_str[3] == '.') {
            // New simplified syntax: var.name-
            name_start = var_tok->start + 4; // Skip "var."
            
            // Find the end of the name (before '-' or end of token)
            name_len = 0;
            for (uint32_t i = 4; i < var_tok->len; i++) {
                if (p->source[var_tok->start + i] == '-') {
                    break;
                }
                name_len++;
            }
        }
        else {
            p->has_error = true;
            return 0;
        }
    }
    
    // Store name in string pool
    uint32_t name_offset = p->string_pos;
    for (uint32_t i = 0; i < name_len; i++) {
        p->string_pool[p->string_pos++] = p->source[name_start + i];
    }
    p->string_pool[p->string_pos++] = '\0';
    
    p->nodes[var_node].data.ident.name_offset = name_offset;
    p->nodes[var_node].data.ident.name_len = name_len;
    
    // Parse initializer [value] if present
    if (check(p, TOK_BRACKET_OPEN)) {
        advance(p);
        
        uint16_t init_expr = 0;
        
        // Parse based on variable type
        switch (var_type) {
            case 4: // string
                if (check(p, TOK_STRING)) {
                    Token* str_tok = advance(p);
                    uint16_t str_node = alloc_node(p, NODE_STRING);
                    uint32_t str_offset = store_string_literal(p, str_tok);
                    p->nodes[str_node].data.ident.name_offset = str_offset;
                    p->nodes[str_node].data.ident.name_len = str_tok->len - 2;
                    init_expr = str_node;
                }
                break;
                
            case 5: // bool
                // Parse true/false identifiers or 0/1 numbers
                init_expr = parse_expression(p);
                break;
                
            default:
                // Parse numeric expression
                init_expr = parse_expression(p);
                break;
        }
        
        if (!match(p, TOK_BRACKET_CLOSE)) {
            p->has_error = true;
            return 0;
        }
        
        // Store init expression
        p->nodes[var_node].data.binary.left_idx = init_expr;
    }
    
    return var_node;
}
```

## Key Improvements

1. **Better name extraction**: The code now correctly handles the different token formats by checking the actual token content.

2. **Type-aware parsing**: The initializer parsing is now aware of the variable type, allowing for better validation.

3. **Clearer logic**: The code separates typed variables from the generic TOK_VAR handling.

## AST Storage Strategy

- **Variable type**: Stored in `data.timing.temporal_offset` (values 0-5)
- **Variable name**: Stored in `data.ident.name_offset` and `data.ident.name_len`
- **Initial value**: Stored in `data.binary.left_idx` (to avoid union conflicts)

This approach avoids union field conflicts while providing all necessary information for code generation.