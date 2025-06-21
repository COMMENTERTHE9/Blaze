// Fixed parse_var_def function for Blaze parser
// Handles all typed variable syntaxes correctly

static uint16_t parse_var_def(Parser* p) {
    Token* var_tok = advance(p); // consume var token
    
    print_str("parse_var_def: token type=");
    print_num(var_tok->type);
    print_str(" len=");
    print_num(var_tok->len);
    print_str(" text='");
    for (uint32_t i = 0; i < var_tok->len; i++) {
        char c[2] = {p->source[var_tok->start + i], '\0'};
        print_str(c);
    }
    print_str("'\n");
    
    uint16_t var_node = alloc_node(p, NODE_VAR_DEF);
    if (var_node == 0) return 0;
    
    // Initialize the node data properly to avoid union conflicts
    p->nodes[var_node].data.timing.expr_idx = 0;
    
    // Store the variable type (we can use timing.temporal_offset for this)
    // 0 = generic var, 1 = const, 2 = int, 3 = float, 4 = string, 5 = bool
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
    
    // Extract variable name from the token
    uint32_t name_start, name_len;
    
    // Check which syntax we have
    if (var_tok->type == TOK_VAR_INT || var_tok->type == TOK_VAR_FLOAT ||
        var_tok->type == TOK_VAR_STRING || var_tok->type == TOK_VAR_BOOL ||
        var_tok->type == TOK_CONST) {
        // Typed variable: var.t-name-
        name_start = var_tok->start + 6; // Skip "var.t-"
        name_len = var_tok->len - 6; // Remove "var.t-"
        
        // Check if it ends with "-" and remove it
        if (name_len > 0 && p->source[var_tok->start + var_tok->len - 1] == '-') {
            name_len--;
        }
        
        print_str("Typed var: prefix_len=6, final name_len=");
        print_num(name_len);
        print_str("\n");
    }
    else if (var_tok->type == TOK_VAR) {
        // TOK_VAR can be old syntax (var.v-name-) or new syntax (var.name-)
        const char* tok_str = &p->source[var_tok->start];
        
        // Check if it's old syntax by looking at position 4 and 5
        if (var_tok->len > 6 && tok_str[4] == 'v' && tok_str[5] == '-') {
            // Old syntax: var.v-name-
            name_start = var_tok->start + 6; // Skip "var.v-"
            name_len = var_tok->len - 6; // Remove "var.v-"
            
            // Remove trailing "-" if present
            if (name_len > 0 && p->source[var_tok->start + var_tok->len - 1] == '-') {
                name_len--;
            }
            
            print_str("Old syntax: prefix_len=6, final name_len=");
            print_num(name_len);
            print_str("\n");
        } 
        else if (var_tok->len > 4 && tok_str[3] == '.') {
            // New simplified syntax: var.name-
            name_start = var_tok->start + 4; // Skip "var."
            
            // Calculate name length by finding where name ends
            // The name ends at the first '-' or at the end of token
            name_len = 0;
            for (uint32_t i = 4; i < var_tok->len; i++) {
                if (p->source[var_tok->start + i] == '-') {
                    break;
                }
                name_len++;
            }
            
            print_str("New syntax: prefix_len=4, calculated name_len=");
            print_num(name_len);
            print_str(" (should be ");
            print_num(var_tok->len - 4 - 1); // What it was doing before
            print_str(" with old calc)\n");
        } else {
            // Unknown syntax
            print_str("ERROR: Unknown var syntax\n");
            p->has_error = true;
            return 0;
        }
    } else {
        // Should not happen
        print_str("ERROR: Unexpected token type in parse_var_def\n");
        p->has_error = true;
        return 0;
    }
    
    // Bounds check
    if (name_len == 0 || name_len > 256) {
        print_str("ERROR: Invalid name length: ");
        print_num(name_len);
        print_str("\n");
        p->has_error = true;
        return 0;
    }
    
    // Store the variable name in string pool
    uint32_t name_offset = p->string_pos;
    print_str("Storing name: '");
    for (uint32_t i = 0; i < name_len; i++) {
        char c = p->source[name_start + i];
        if (p->string_pos >= 4096) {
            print_str("ERROR: String pool overflow\n");
            p->has_error = true;
            return 0;
        }
        p->string_pool[p->string_pos++] = c;
        char str[2] = {c, '\0'};
        print_str(str);
    }
    print_str("' at offset ");
    print_num(name_offset);
    print_str("\n");
    p->string_pool[p->string_pos++] = '\0';
    
    p->nodes[var_node].data.ident.name_offset = name_offset;
    p->nodes[var_node].data.ident.name_len = name_len;
    
    print_str("Stored in node: offset=");
    print_num(name_offset);
    print_str(" len=");
    print_num(name_len);
    print_str("\n");
    
    // Check for initializer value in brackets [value]
    if (check(p, TOK_BRACKET_OPEN)) {
        advance(p); // consume [
        
        print_str("Parsing initializer for type ");
        print_num(var_type);
        print_str("\n");
        
        // Parse the value(s) inside brackets
        uint16_t init_expr = 0;
        
        // Type-specific parsing
        if (var_type == 4 && check(p, TOK_STRING)) {
            // String variable with string literal
            Token* str_tok = advance(p);
            uint16_t str_node = alloc_node(p, NODE_STRING);
            if (str_node == 0) return 0;
            
            // Store string content
            uint32_t str_offset = store_string_literal(p, str_tok);
            p->nodes[str_node].data.ident.name_offset = str_offset;
            p->nodes[str_node].data.ident.name_len = str_tok->len - 2; // Remove quotes
            init_expr = str_node;
        }
        else if (var_type == 5) {
            // Boolean variable - accept true/false identifiers or 0/1
            if (check(p, TOK_IDENTIFIER)) {
                Token* bool_tok = peek(p);
                // Check if it's "true" or "false"
                const char* bool_str = &p->source[bool_tok->start];
                if (bool_tok->len == 4 && str_equals(bool_str, "true", 4)) {
                    advance(p);
                    init_expr = alloc_node(p, NODE_NUMBER);
                    p->nodes[init_expr].data.number = 1;
                } else if (bool_tok->len == 5 && str_equals(bool_str, "false", 5)) {
                    advance(p);
                    init_expr = alloc_node(p, NODE_NUMBER);
                    p->nodes[init_expr].data.number = 0;
                } else {
                    // Not a boolean literal, parse as expression
                    init_expr = parse_expression(p);
                }
            } else {
                // Parse as expression (could be 0 or 1)
                init_expr = parse_expression(p);
            }
        }
        else {
            // For all other types, parse as expression
            init_expr = parse_expression(p);
        }
        
        // Expect closing bracket
        if (!match(p, TOK_BRACKET_CLOSE)) {
            print_str("ERROR: Expected closing bracket\n");
            p->has_error = true;
            return 0;
        }
        
        // Store init expression using binary.left_idx which doesn't overlap with ident
        if (init_expr != 0) {
            p->nodes[var_node].data.binary.left_idx = init_expr;
            print_str("Stored init expression in node ");
            print_num(init_expr);
            print_str("\n");
        }
    }
    
    return var_node;
}