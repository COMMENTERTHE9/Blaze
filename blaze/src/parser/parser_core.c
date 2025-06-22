// BLAZE PARSER - Token to AST conversion
// Stack-based recursive descent parser with no heap allocation

#include "blaze_internals.h"

// Optimized string comparison - no strlen needed
static inline bool str_equals(const char* a, const char* b, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

// Safe string pool write with bounds checking
static inline bool safe_string_pool_write(Parser* p, char c) {
    if (p->string_pos >= 4095) { // Leave room for null terminator
        p->has_error = true;
        return false;
    }
    p->string_pool[p->string_pos++] = c;
    return true;
}

// Parser state is now defined in blaze_internals.h

// Global flag to track if we're inside a declare block
static bool in_declare_block = false;

// Forward declarations
static uint16_t parse_expression(Parser* p);
static uint16_t parse_statement(Parser* p);
static uint16_t parse_pipe_func_def(Parser* p);
static uint16_t parse_action_block(Parser* p);
static uint16_t parse_solid_number(Parser* p);
// Removed parse_declare_block - handled inline now

// Parser utilities
static inline bool at_end(Parser* p) {
    return p->current >= p->count || p->tokens[p->current].type == TOK_EOF;
}

static inline Token* peek(Parser* p) {
    if (at_end(p)) return NULL;
    return &p->tokens[p->current];
}

static inline Token* peek2(Parser* p) {
    if (p->current + 1 >= p->count) return NULL;
    return &p->tokens[p->current + 1];
}

static inline Token* peek3(Parser* p) {
    if (p->current + 2 >= p->count) return NULL;
    return &p->tokens[p->current + 2];
}

static inline Token* advance(Parser* p) {
    if (!at_end(p)) p->current++;
    return &p->tokens[p->current - 1];
}

static inline bool check(Parser* p, TokenType type) {
    if (at_end(p)) return false;
    return peek(p)->type == type;
}

static bool match(Parser* p, TokenType type) {
    if (check(p, type)) {
        advance(p);
        return true;
    }
    return false;
}

// Allocate AST node from pool
static uint16_t alloc_node(Parser* p, NodeType type) {
    if (p->node_count >= p->node_capacity) {
        print_str("[ALLOC] ERROR: node_count=");
        print_num(p->node_count);
        print_str(" >= capacity=");
        print_num(p->node_capacity);
        print_str("\n");
        p->has_error = true;
        return 0;
    }
    
    uint16_t idx = p->node_count++;
    ASTNode* node = &p->nodes[idx];
    node->type = type;
    
    // Disable verbose allocation debug for now
    
    // Zero out data union - the union is larger than 2 uint64_t
    // Clear the entire union (max size is the inline_asm struct)
    uint8_t* data = (uint8_t*)&node->data;
    for (int i = 0; i < sizeof(node->data); i++) {
        data[i] = 0;
    }
    
    return idx;
}

// Store string in pool
static uint32_t store_string(Parser* p, Token* tok) {
    uint32_t offset = p->string_pos;
    
    // Check if we have enough space for the string + null terminator
    if (p->string_pos + tok->len + 1 > 4096) {
        p->has_error = true;
        return 0;
    }
    
    // Copy token text to string pool
    for (uint16_t i = 0; i < tok->len; i++) {
        p->string_pool[p->string_pos++] = p->source[tok->start + i];
    }
    
    // Null terminate
    p->string_pool[p->string_pos++] = '\0';
    
    return offset;
}

// Store string literal (removes quotes and processes escapes)
static uint32_t store_string_literal(Parser* p, Token* tok) {
    uint32_t offset = p->string_pos;
    
    // Skip opening quote
    uint16_t start = 1;
    uint16_t end = tok->len - 1; // Skip closing quote
    
    // Check worst case: every character could be an escape + null terminator
    if (p->string_pos + (end - start) + 1 > 4096) {
        p->has_error = true;
        return 0;
    }
    
    // Copy string, processing escapes
    for (uint16_t i = start; i < end; i++) {
        // Check bounds before each write
        if (p->string_pos >= 4095) { // Leave room for null terminator
            p->has_error = true;
            return 0;
        }
        
        char c = p->source[tok->start + i];
        if (c == '\\' && i + 1 < end) {
            // Handle escape sequences
            i++;
            c = p->source[tok->start + i];
            switch (c) {
                case 'n': p->string_pool[p->string_pos++] = '\n'; break;
                case 't': p->string_pool[p->string_pos++] = '\t'; break;
                case 'r': p->string_pool[p->string_pos++] = '\r'; break;
                case '\\': p->string_pool[p->string_pos++] = '\\'; break;
                case '"': p->string_pool[p->string_pos++] = '"'; break;
                default: p->string_pool[p->string_pos++] = c; break;
            }
        } else {
            p->string_pool[p->string_pos++] = c;
        }
    }
    
    // Null terminate (we already checked bounds)
    p->string_pool[p->string_pos++] = '\0';
    
    return offset;
}

// Parse number literal
static uint16_t parse_number(Parser* p) {
    Token* num_tok = advance(p);
    
    print_str("[PARSE_NUMBER] Token start=");
    print_num(num_tok->start);
    print_str(" len=");
    print_num(num_tok->len);
    print_str(" value=");
    for (uint16_t i = 0; i < num_tok->len; i++) {
        char c = p->source[num_tok->start + i];
        char buf[2] = {c, 0};
        print_str(buf);
    }
    print_str("\n");
    
    // Check if it's a float (contains . or e/E)
    bool is_float = false;
    for (uint16_t i = 0; i < num_tok->len; i++) {
        char c = p->source[num_tok->start + i];
        if (c == '.' || c == 'e' || c == 'E') {
            is_float = true;
            print_str("[PARSE_NUMBER] Found float indicator at pos ");
            print_num(i);
            print_str("\n");
            break;
        }
    }
    
    if (is_float) {
        // Creating NODE_FLOAT
        uint16_t node_idx = alloc_node(p, NODE_FLOAT);
        if (node_idx == 0) return 0;
        
        // Convert string to float
        double value = 0.0;
        double decimal_place = 0.1;
        bool after_decimal = false;
        bool in_exponent = false;
        int exponent = 0;
        bool exp_negative = false;
        
        for (uint16_t i = 0; i < num_tok->len; i++) {
            char c = p->source[num_tok->start + i];
            
            if (c == '.') {
                after_decimal = true;
            } else if (c == 'e' || c == 'E') {
                in_exponent = true;
                after_decimal = false;
            } else if (c == '+' && in_exponent) {
                // Skip positive exponent sign
            } else if (c == '-' && in_exponent) {
                exp_negative = true;
            } else if (c >= '0' && c <= '9') {
                if (in_exponent) {
                    exponent = exponent * 10 + (c - '0');
                } else if (after_decimal) {
                    value += (c - '0') * decimal_place;
                    decimal_place *= 0.1;
                } else {
                    value = value * 10.0 + (c - '0');
                }
            }
        }
        
        // Apply exponent if present
        if (in_exponent) {
            if (exp_negative) exponent = -exponent;
            double multiplier = 1.0;
            for (int i = 0; i < (exponent < 0 ? -exponent : exponent); i++) {
                multiplier *= 10.0;
            }
            value = exponent < 0 ? value / multiplier : value * multiplier;
        }
        
        p->nodes[node_idx].data.float_value = value;
        // Stored float value
        return node_idx;
    } else {
        // Creating NODE_NUMBER
        uint16_t node_idx = alloc_node(p, NODE_NUMBER);
        if (node_idx == 0) return 0;
        
        // Convert string to integer
        int64_t value = 0;
        for (uint16_t i = 0; i < num_tok->len; i++) {
            char c = p->source[num_tok->start + i];
            if (c >= '0' && c <= '9') {
                value = value * 10 + (c - '0');
            }
        }
        
        p->nodes[node_idx].data.number = value;
        return node_idx;
    }
}

// Parse solid number
static uint16_t parse_solid_number(Parser* p) {
    Token* tok = advance(p);
    uint16_t node = alloc_node(p, NODE_SOLID);
    if (node == 0) return 0;
    
    const char* input = p->source + tok->start;
    uint32_t pos = 0;
    uint32_t len = tok->len;
    
    // Parse known digits (before first ...)
    uint32_t known_start = pos;
    while (pos < len && input[pos] != '.') {
        pos++;
    }
    uint32_t known_len = pos;
    
    // Store known digits in string pool
    uint32_t known_offset = p->string_pos;
    if (p->string_pos + known_len + 1 > 4096) {
        p->has_error = true;
        return 0;
    }
    
    for (uint32_t i = 0; i < known_len; i++) {
        p->string_pool[p->string_pos++] = input[i];
    }
    p->string_pool[p->string_pos++] = '\0';
    
    p->nodes[node].data.solid.known_offset = known_offset;
    p->nodes[node].data.solid.known_len = known_len;
    
    // Skip first "..."
    if (pos + 2 < len && input[pos] == '.' && input[pos+1] == '.' && input[pos+2] == '.') {
        pos += 3;
    }
    
    // Parse barrier spec in (...)
    if (pos < len && input[pos] == '(') {
        pos++; // skip '('
        
        // Check for "exact"
        if (pos + 5 <= len && input[pos] == 'e' && input[pos+1] == 'x' && 
            input[pos+2] == 'a' && input[pos+3] == 'c' && input[pos+4] == 't') {
            p->nodes[node].data.solid.barrier_type = 'x';
            p->nodes[node].data.solid.gap_magnitude = 0;
            p->nodes[node].data.solid.confidence_x1000 = 1000; // 100%
            pos += 5;
        } else {
            // Parse barrier type
            if (pos < len) {
                char barrier = input[pos];
                if (barrier == 'q' || barrier == 'e' || barrier == 's' || 
                    barrier == 't' || barrier == 'c' || barrier == 'u') {
                    p->nodes[node].data.solid.barrier_type = barrier;
                    pos++;
                } else if (pos + 2 < len && (unsigned char)input[pos] == 0xE2 && 
                          (unsigned char)input[pos+1] == 0x88 && (unsigned char)input[pos+2] == 0x9E) {
                    // UTF-8 infinity ∞
                    p->nodes[node].data.solid.barrier_type = 'i'; // Use 'i' for infinity internally
                    pos += 3;
                } else if (pos + 3 <= len && input[pos] == 'i' && input[pos+1] == 'n' && input[pos+2] == 'f') {
                    p->nodes[node].data.solid.barrier_type = 'i';
                    pos += 3;
                }
            }
            
            // Parse colon
            if (pos < len && input[pos] == ':') {
                pos++;
                
                // Parse gap magnitude
                uint64_t gap = 0;
                bool is_infinity = false;
                
                if (pos + 2 < len && input[pos] == '1' && input[pos+1] == '0') {
                    // Parse 10^n format
                    pos += 2;
                    if (pos < len && input[pos] == '^') {
                        pos++;
                        // Parse exponent
                        uint32_t exp = 0;
                        while (pos < len && input[pos] >= '0' && input[pos] <= '9') {
                            exp = exp * 10 + (input[pos] - '0');
                            pos++;
                        }
                        // Calculate 10^exp (simplified - in real implementation use proper method)
                        gap = 1;
                        for (uint32_t i = 0; i < exp; i++) {
                            gap *= 10;
                        }
                    }
                } else if ((pos + 2 < len && (unsigned char)input[pos] == 0xE2 && 
                           (unsigned char)input[pos+1] == 0x88 && (unsigned char)input[pos+2] == 0x9E) ||
                          (pos + 3 <= len && input[pos] == 'i' && input[pos+1] == 'n' && input[pos+2] == 'f')) {
                    is_infinity = true;
                    gap = ~0ULL; // All bits set = max value
                    pos += (input[pos] == 'i') ? 3 : 3;
                }
                
                p->nodes[node].data.solid.gap_magnitude = gap;
                
                // Parse optional confidence
                if (pos < len && input[pos] == '|') {
                    pos++;
                    // Parse confidence as decimal
                    uint32_t conf_int = 0;
                    uint32_t conf_frac = 0;
                    uint32_t frac_digits = 0;
                    
                    // Integer part
                    while (pos < len && input[pos] >= '0' && input[pos] <= '9') {
                        conf_int = conf_int * 10 + (input[pos] - '0');
                        pos++;
                    }
                    
                    // Fractional part
                    if (pos < len && input[pos] == '.') {
                        pos++;
                        while (pos < len && input[pos] >= '0' && input[pos] <= '9') {
                            conf_frac = conf_frac * 10 + (input[pos] - '0');
                            frac_digits++;
                            pos++;
                        }
                    }
                    
                    // Convert to x1000 format
                    uint32_t confidence = conf_int * 1000;
                    for (uint32_t i = 0; i < frac_digits && i < 3; i++) {
                        uint32_t divisor = 1;
                        for (uint32_t j = i + 1; j < frac_digits; j++) divisor *= 10;
                        confidence += (conf_frac / divisor) * (i == 0 ? 100 : i == 1 ? 10 : 1);
                    }
                    
                    p->nodes[node].data.solid.confidence_x1000 = confidence;
                } else {
                    p->nodes[node].data.solid.confidence_x1000 = 1000; // Default 100%
                }
            }
        }
        
        // Skip closing ')'
        if (pos < len && input[pos] == ')') {
            pos++;
        }
    }
    
    // Skip second "..."
    if (pos + 2 < len && input[pos] == '.' && input[pos+1] == '.' && input[pos+2] == '.') {
        pos += 3;
    }
    
    // Parse terminal
    uint32_t terminal_start = pos;
    uint32_t terminal_len = len - pos;
    uint8_t terminal_type = 0; // default: digits
    
    // Check for special terminals
    if (terminal_len >= 3 && input[pos] == '{' && input[pos+1] == '*' && input[pos+2] == '}') {
        terminal_type = 2; // superposition
        terminal_len = 3;
    } else if ((terminal_len >= 3 && (unsigned char)input[pos] == 0xE2 && 
               (unsigned char)input[pos+1] == 0x88 && (unsigned char)input[pos+2] == 0x85) ||
              (terminal_len >= 4 && input[pos] == 'n' && input[pos+1] == 'u' && 
               input[pos+2] == 'l' && input[pos+3] == 'l')) {
        terminal_type = 1; // undefined
        terminal_len = (input[pos] == 'n') ? 4 : 3;
    }
    
    // Store terminal in string pool
    uint32_t terminal_offset = p->string_pos;
    if (p->string_pos + terminal_len + 1 > 4096) {
        p->has_error = true;
        return 0;
    }
    
    for (uint32_t i = 0; i < terminal_len; i++) {
        p->string_pool[p->string_pos++] = input[terminal_start + i];
    }
    p->string_pool[p->string_pos++] = '\0';
    
    p->nodes[node].data.solid.terminal_offset = terminal_offset;
    p->nodes[node].data.solid.terminal_len = terminal_len;
    p->nodes[node].data.solid.terminal_type = terminal_type;
    
    return node;
}

// Parse identifier
static uint16_t parse_identifier(Parser* p) {
    Token* id_tok = advance(p);
    uint16_t node_idx = alloc_node(p, NODE_IDENTIFIER);
    
    if (node_idx == 0) return 0;
    
    uint32_t str_offset = store_string(p, id_tok);
    p->nodes[node_idx].data.ident.name_offset = str_offset;
    p->nodes[node_idx].data.ident.name_len = id_tok->len;
    
    return node_idx;
}

// Parse timing operation (time-travel)
static uint16_t parse_timing_op(Parser* p) {
    Token* op_tok = advance(p);
    uint16_t node_idx = alloc_node(p, NODE_TIMING_OP);
    
    if (node_idx == 0) return 0;
    
    p->nodes[node_idx].data.timing.timing_op = op_tok->type;
    
    // Parse the expression this timing applies to
    uint16_t expr_idx = parse_expression(p);
    p->nodes[node_idx].data.timing.expr_idx = expr_idx;
    
    // Calculate temporal offset based on operator
    switch (op_tok->type) {
        case TOK_TIMING_ONTO:  // <<
            p->nodes[node_idx].data.timing.temporal_offset = -1;
            break;
        case TOK_TIMING_INTO:  // >>
            p->nodes[node_idx].data.timing.temporal_offset = 1;
            break;
        case TOK_TIMING_BOTH:  // <>
            p->nodes[node_idx].data.timing.temporal_offset = 0;
            break;
        default:
            p->nodes[node_idx].data.timing.temporal_offset = 0;
    }
    
    return node_idx;
}

// Parse primary expression
static uint16_t parse_primary(Parser* p) {
    // Numbers
    if (check(p, TOK_NUMBER)) {
        return parse_number(p);
    }
    
    // Solid numbers
    if (check(p, TOK_SOLID_NUMBER)) {
        return parse_solid_number(p);
    }
    
    // Math functions: math.sin(x), math.cos(x), etc.
    if (check(p, TOK_MATH_PREFIX)) {
        advance(p); // consume "math."
        
        // Next should be function name
        if (!check(p, TOK_IDENTIFIER)) {
            p->has_error = true;
            return 0;
        }
        
        Token* func_name = advance(p);
        
        // Next should be opening parenthesis
        if (!match(p, TOK_LPAREN)) {
            p->has_error = true;
            return 0;
        }
        
        // Parse the argument
        uint16_t arg = parse_expression(p);
        
        // Closing parenthesis
        if (!match(p, TOK_RPAREN)) {
            p->has_error = true;
            return 0;
        }
        
        // Create a function call node
        uint16_t call_node = alloc_node(p, NODE_FUNC_CALL);
        if (call_node == 0) return 0;
        
        // Store function name in string pool
        uint32_t name_offset = p->string_pos;
        if (p->string_pos + func_name->len + 1 > 4096) {
            p->has_error = true;
            return 0;
        }
        
        for (uint32_t i = 0; i < func_name->len; i++) {
            p->string_pool[p->string_pos++] = p->source[func_name->start + i];
        }
        p->string_pool[p->string_pos++] = '\0';
        
        // Create identifier node for function name
        uint16_t name_node = alloc_node(p, NODE_IDENTIFIER);
        if (name_node == 0) return 0;
        
        p->nodes[name_node].data.ident.name_offset = name_offset;
        p->nodes[name_node].data.ident.name_len = func_name->len;
        // Math function is identified by being in a NODE_FUNC_CALL with math. prefix
        
        // Store function name in left, argument in right
        p->nodes[call_node].data.binary.left_idx = name_node;
        p->nodes[call_node].data.binary.right_idx = arg;
        
        return call_node;
    }
    
    // Identifiers and array access
    if (check(p, TOK_IDENTIFIER)) {
        uint16_t id_node = parse_identifier(p);
        
        // Check for array access [x, y, z, t] or [x, y, z, <t]
        if (check(p, TOK_BRACKET_OPEN)) {
            advance(p); // consume [
            
            uint16_t access_node = alloc_node(p, NODE_ARRAY_4D_ACCESS);
            if (access_node == 0) return 0;
            
            p->nodes[access_node].data.array_4d.name_idx = id_node;
            
            // Parse indices
            for (int i = 0; i < 4; i++) {
                // Check for temporal operators on last dimension
                if (i == 3 && (check(p, TOK_LT) || check(p, TOK_GT) || 
                              check(p, TOK_TIMING_ONTO) || check(p, TOK_TIMING_INTO))) {
                    // Temporal access like <t or >t
                    uint16_t temp_op = parse_timing_op(p);
                    p->nodes[access_node].data.array_4d.dim_indices[i] = temp_op;
                } else {
                    uint16_t idx_expr = parse_expression(p);
                    p->nodes[access_node].data.array_4d.dim_indices[i] = idx_expr;
                }
                
                if (i < 3 && !match(p, TOK_COMMA)) {
                    p->has_error = true;
                    return 0;
                }
            }
            
            if (!match(p, TOK_BRACKET_CLOSE)) {
                p->has_error = true;
                return 0;
            }
            
            return access_node;
        }
        
        return id_node;
    }
    
    // Pipe-enclosed expressions |expr|
    if (match(p, TOK_PIPE)) {
        uint16_t expr = parse_expression(p);
        if (!match(p, TOK_PIPE)) {
            p->has_error = true;
            return 0;
        }
        return expr;
    }
    
    // Bracket expressions [expr]
    if (match(p, TOK_BRACKET_OPEN)) {
        uint16_t expr = parse_expression(p);
        if (!match(p, TOK_BRACKET_CLOSE)) {
            p->has_error = true;
            return 0;
        }
        return expr;
    }
    
    // Parenthesized expressions (expr)
    if (match(p, TOK_LPAREN)) {
        uint16_t expr = parse_expression(p);
        if (!match(p, TOK_RPAREN)) {
            p->has_error = true;
            return 0;
        }
        return expr;
    }
    
    // String literals
    if (check(p, TOK_STRING)) {
        Token* str_tok = advance(p);
        uint16_t str_node = alloc_node(p, NODE_STRING);
        if (str_node == 0) return 0;
        
        uint32_t str_offset = store_string_literal(p, str_tok);
        p->nodes[str_node].data.ident.name_offset = str_offset;
        p->nodes[str_node].data.ident.name_len = str_tok->len - 2; // Remove quotes
        return str_node;
    }
    
    // Parameters {@param:name}
    if (check(p, TOK_PARAM) || (check(p, TOK_LBRACE) && peek2(p) && peek2(p)->type == TOK_AT)) {
        advance(p); // Skip parameter token
        // For now, return a placeholder identifier
        uint16_t param_node = alloc_node(p, NODE_IDENTIFIER);
        p->nodes[param_node].data.ident.name_offset = 0;
        p->nodes[param_node].data.ident.name_len = 5; // "param"
        return param_node;
    }
    
    p->has_error = true;
    return 0;
}

// Parse binary operation
static uint16_t parse_binary(Parser* p, uint16_t left, TokenType op) {
    uint16_t node_idx = alloc_node(p, NODE_BINARY_OP);
    if (node_idx == 0) return 0;
    
    p->nodes[node_idx].data.binary.op = op;
    p->nodes[node_idx].data.binary.left_idx = left;
    p->nodes[node_idx].data.binary.right_idx = parse_expression(p);
    
    return node_idx;
}

// Parse expression with precedence
static uint16_t parse_expression_prec(Parser* p, int min_prec);

// Get operator precedence
static int get_precedence(TokenType type) {
    switch (type) {
        // Exponentiation has highest precedence
        case TOK_EXPONENT:
            return 7;
            
        // Multiplication and division
        case TOK_STAR:
        case TOK_DIV:
        case TOK_PERCENT:
            return 6;
            
        // Addition and subtraction
        case TOK_PLUS:
        case TOK_MINUS:
            return 5;
            
        // Comparison operators
        case TOK_LT_CMP:
        case TOK_GT_CMP:
        case TOK_LE:
        case TOK_GE:
            return 4;
            
        // Equality operators
        case TOK_EQ:
        case TOK_NE:
            return 3;
            
        // Blaze comparison operators
        case TOK_GREATER_THAN:  // *>
        case TOK_LESS_EQUAL:    // *_<
        case TOK_EQUAL:         // *=
        case TOK_NOT_EQUAL:     // *!=
            return 3;
            
        // Bitwise operators (higher precedence than logical)
        case TOK_BIT_LSHIFT:
        case TOK_BIT_RSHIFT:
            return 4;  // Same as comparison
            
        case TOK_BIT_AND:
            return 3;  // Between comparison and logical
            
        case TOK_BIT_XOR:
            return 3;
            
        case TOK_BIT_OR:
            return 3;
            
        // Logical AND
        case TOK_AND:
            return 2;
            
        // Logical OR
        case TOK_OR:
            return 1;
            
        // Pipe operator (lower precedence for data flow)
        case TOK_PIPE:
            return 0;
            
        // Temporal operators (lowest precedence)
        case TOK_LT:
        case TOK_GT:
        case TOK_TIMING_ONTO:
        case TOK_TIMING_INTO:
        case TOK_TIMING_BOTH:
            return 1;
            
        default:
            return 0;
    }
}

// Check if operator is right associative
static bool is_right_assoc(TokenType type) {
    // Exponentiation is right associative (like in most languages)
    if (type == TOK_EXPONENT) return true;
    
    // Most operators are left associative
    return false;
}

// Parse expression with precedence climbing
static uint16_t parse_expression_prec(Parser* p, int min_prec) {
    // Parse left side (primary or unary expression)
    uint16_t left;
    
    // Check for unary operators
    if (check(p, TOK_MINUS)) {
        advance(p);
        // Parse unary minus
        uint16_t expr = parse_expression_prec(p, get_precedence(TOK_MINUS));
        uint16_t neg_node = alloc_node(p, NODE_BINARY_OP);
        if (neg_node == 0) return 0;
        
        // Create 0 - expr
        // Check if the expression is a float to create appropriate zero
        uint16_t zero_node = alloc_node(p, NODE_NUMBER);
        if (zero_node == 0) return 0;
        
        // If the expression is a float, create a float zero
        if (expr != 0 && p->nodes[expr].type == NODE_FLOAT) {
            p->nodes[zero_node].type = NODE_FLOAT;
            p->nodes[zero_node].data.float_value = 0.0;
        } else {
            p->nodes[zero_node].data.number = 0;
        }
        
        p->nodes[neg_node].data.binary.op = TOK_MINUS;
        p->nodes[neg_node].data.binary.left_idx = zero_node;
        p->nodes[neg_node].data.binary.right_idx = expr;
        left = neg_node;
    } else if (check(p, TOK_BANG)) {
        advance(p);
        // Parse logical NOT
        uint16_t expr = parse_expression_prec(p, 10); // High precedence for unary
        uint16_t not_node = alloc_node(p, NODE_UNARY_OP);
        if (not_node == 0) return 0;
        
        p->nodes[not_node].data.unary.op = TOK_BANG;
        p->nodes[not_node].data.unary.expr_idx = expr;
        left = not_node;
    } else if (check(p, TOK_BIT_NOT)) {
        advance(p);
        // Parse bitwise NOT
        uint16_t expr = parse_expression_prec(p, 10); // High precedence for unary
        uint16_t not_node = alloc_node(p, NODE_UNARY_OP);
        if (not_node == 0) return 0;
        
        p->nodes[not_node].data.unary.op = TOK_BIT_NOT;
        p->nodes[not_node].data.unary.expr_idx = expr;
        left = not_node;
    } else if (check(p, TOK_LT) || check(p, TOK_GT) || 
               check(p, TOK_TIMING_ONTO) || check(p, TOK_TIMING_INTO) || 
               check(p, TOK_TIMING_BOTH)) {
        // Timing operators
        left = parse_timing_op(p);
    } else {
        // Primary expression
        left = parse_primary(p);
    }
    
    // Parse binary operators
    while (!at_end(p)) {
        TokenType op_type = TOK_EOF;
        
        // Check for operators
        if (check(p, TOK_STAR)) {
            // Could be multiplication or Blaze comparison
            Token* next = peek2(p);
            if (next && (next->type == TOK_GT || next->type == TOK_EQUALS ||
                        (next->type == TOK_UNDERSCORE || next->type == TOK_BANG))) {
                // It's a Blaze comparison operator
                advance(p); // consume *
                if (match(p, TOK_GT)) {
                    op_type = TOK_GREATER_THAN;
                } else if (match(p, TOK_UNDERSCORE) && match(p, TOK_LT)) {
                    op_type = TOK_LESS_EQUAL;
                } else if (match(p, TOK_EQUALS)) {
                    op_type = TOK_EQUAL;
                } else if (match(p, TOK_BANG) && match(p, TOK_EQUALS)) {
                    op_type = TOK_NOT_EQUAL;
                }
            } else {
                // It's multiplication
                op_type = TOK_STAR;
            }
        } else if (check(p, TOK_PLUS)) {
            op_type = TOK_PLUS;
        } else if (check(p, TOK_MINUS)) {
            op_type = TOK_MINUS;
        } else if (check(p, TOK_DIV)) {
            op_type = TOK_DIV;
        } else if (check(p, TOK_PERCENT)) {
            op_type = TOK_PERCENT;
        } else if (check(p, TOK_EXPONENT)) {
            op_type = TOK_EXPONENT;
        } else if (check(p, TOK_LT_CMP)) {
            op_type = TOK_LT_CMP;
        } else if (check(p, TOK_GT_CMP)) {
            op_type = TOK_GT_CMP;
        } else if (check(p, TOK_LE)) {
            op_type = TOK_LE;
        } else if (check(p, TOK_GE)) {
            op_type = TOK_GE;
        } else if (check(p, TOK_EQ)) {
            op_type = TOK_EQ;
        } else if (check(p, TOK_NE)) {
            op_type = TOK_NE;
        } else if (check(p, TOK_PIPE)) {
            op_type = TOK_PIPE;
        } else if (check(p, TOK_AND)) {
            op_type = TOK_AND;
        } else if (check(p, TOK_OR)) {
            op_type = TOK_OR;
        } else if (check(p, TOK_BIT_AND)) {
            op_type = TOK_BIT_AND;
        } else if (check(p, TOK_BIT_OR)) {
            op_type = TOK_BIT_OR;
        } else if (check(p, TOK_BIT_XOR)) {
            op_type = TOK_BIT_XOR;
        } else if (check(p, TOK_BIT_LSHIFT)) {
            op_type = TOK_BIT_LSHIFT;
        } else if (check(p, TOK_BIT_RSHIFT)) {
            op_type = TOK_BIT_RSHIFT;
        } else if (check(p, TOK_BANG)) {
            // Handle logical NOT as unary operator
            // This should be handled in parse_primary, not here
            break;
        } else {
            // No more operators
            break;
        }
        
        if (op_type == TOK_EOF) break;
        
        print_str("[EXPR] Found operator type=");
        print_num(op_type);
        print_str("\n");
        
        int prec = get_precedence(op_type);
        if (prec < min_prec) break;
        
        // Consume the operator
        if (op_type != TOK_GREATER_THAN && op_type != TOK_LESS_EQUAL && 
            op_type != TOK_EQUAL && op_type != TOK_NOT_EQUAL) {
            advance(p);
        }
        
        // Parse right side with appropriate precedence
        int next_min_prec = is_right_assoc(op_type) ? prec : prec + 1;
        uint16_t right = parse_expression_prec(p, next_min_prec);
        
        // Create binary node
        uint16_t bin_node = alloc_node(p, NODE_BINARY_OP);
        if (bin_node == 0) return 0;
        
        p->nodes[bin_node].data.binary.op = op_type;
        p->nodes[bin_node].data.binary.left_idx = left;
        p->nodes[bin_node].data.binary.right_idx = right;
        
        left = bin_node;
    }
    
    return left;
}

// Main expression parser entry point
static uint16_t parse_expression(Parser* p) {
    return parse_expression_prec(p, 0);
}

// Parse 4D array definition: array.4d[x, y, z, t]
static uint16_t parse_array_4d_def(Parser* p) {
    advance(p); // consume array.4d
    
    if (!check(p, TOK_IDENTIFIER)) {
        p->has_error = true;
        return 0;
    }
    
    uint16_t array_node = alloc_node(p, NODE_ARRAY_4D_DEF);
    if (array_node == 0) return 0;
    
    // Get array name
    Token* name_tok = advance(p);
    uint32_t name_offset = store_string(p, name_tok);
    p->nodes[array_node].data.array_4d.name_idx = p->node_count;
    
    // Create identifier node for array name
    uint16_t name_node = alloc_node(p, NODE_IDENTIFIER);
    p->nodes[name_node].data.ident.name_offset = name_offset;
    p->nodes[name_node].data.ident.name_len = name_tok->len;
    
    // Parse dimensions [x, y, z, t]
    if (!match(p, TOK_BRACKET_OPEN)) {
        p->has_error = true;
        return 0;
    }
    
    // Parse 4 dimension expressions
    for (int i = 0; i < 4; i++) {
        uint16_t dim_expr = parse_expression(p);
        
        // Store dimension expressions in the dim_indices array
        p->nodes[array_node].data.array_4d.dim_indices[i] = dim_expr;
        
        if (i < 3 && !match(p, TOK_COMMA)) {
            p->has_error = true;
            return 0;
        }
    }
    
    if (!match(p, TOK_BRACKET_CLOSE)) {
        p->has_error = true;
        return 0;
    }
    
    return array_node;
}

// Parse variable definition
static uint16_t parse_var_def(Parser* p) {
    Token* var_tok = advance(p); // consume var token
    
    // Parse variable definition
    
    uint16_t var_node = alloc_node(p, NODE_VAR_DEF);
    if (var_node == 0) {
        print_str("[PARSER] Failed to allocate node\n");
        return 0;
    }
    // Allocated node
    
    // Store the variable type
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
        
    }
    else if (var_tok->len > 6 && str_equals(&p->source[var_tok->start], "var.v-", 6)) {
        // Old syntax: var.v-name-
        name_start = var_tok->start + 6; // Skip "var.v-"
        name_len = var_tok->len - 6; // Remove "var.v-"
        
        // Check if it ends with "-" and remove it
        if (name_len > 0 && p->source[var_tok->start + var_tok->len - 1] == '-') {
            name_len--;
        }
        
    } else if (var_tok->len > 4 && str_equals(&p->source[var_tok->start], "var.", 4)) {
        // New simplified syntax: var.name-
        name_start = var_tok->start + 4; // Skip "var."
        
        // Calculate name length by finding the '-'
        name_len = 0;
        for (uint32_t i = 4; i < var_tok->len; i++) {
            if (p->source[var_tok->start + i] == '-') {
                break;
            }
            name_len++;
        }
        
    } else {
        // Unknown syntax
        p->has_error = true;
        return 0;
    }
    
    
    // Bounds check
    if (name_len == 0 || name_len > 256) {
        p->has_error = true;
        return 0;
    }
    
    // Store the variable name in string pool
    uint32_t name_offset = p->string_pos;
    
    // Check if we have enough space for the name + null terminator
    if (p->string_pos + name_len + 1 > 4096) {
        p->has_error = true;
        return 0;
    }
    
    for (uint32_t i = 0; i < name_len; i++) {
        char c = p->source[name_start + i];
        p->string_pool[p->string_pos++] = c;
    }
    p->string_pool[p->string_pos++] = '\0';
    
    // Store name in string pool
    
    p->nodes[var_node].data.ident.name_offset = name_offset;
    p->nodes[var_node].data.ident.name_len = name_len;
    
    // Store packed data in a separate field for now
    // We'll use the timing.temporal_offset field which is 32-bit
    p->nodes[var_node].data.timing.temporal_offset = 0;  // Clear it first
    
    // Don't store var_type in timing_op - it overlaps with ident fields!
    // Instead, we'll determine var_type from the token type at codegen time
    
    print_str("[PARSER] Created NODE_VAR_DEF at idx=");
    print_num(var_node);
    print_str(" name_offset=");
    print_num(name_offset);
    print_str(" name_len=");
    print_num(name_len);
    print_str("\n");
    
    // Packed var_type into upper bits
    
    
    // Check for initializer value in brackets [value]
    if (check(p, TOK_BRACKET_OPEN)) {
        advance(p); // consume [
        
        // Parse the value(s) inside brackets - can be any expression
        uint16_t init_expr = 0;
        
        if (check(p, TOK_BRACKET_CLOSE)) {
            // Empty brackets [] - no initializer
            init_expr = 0;
        } else {
            // Parse any expression (number, identifier, binary op, etc.)
            init_expr = parse_expression(p);
            if (init_expr == 0) {
                p->has_error = true;
                return 0;
            }
        }
        
        // Expect closing bracket
        if (!match(p, TOK_BRACKET_CLOSE)) {
            print_str("[PARSER] Expected closing bracket ]\n");
            p->has_error = true;
            return 0;
        }
        
        // Store init expression in the upper 16 bits of name_len
        // Bits 0-15: actual name length
        // Bits 16-31: init expression index
        if (init_expr != 0) {
            print_str("[PARSER] Storing init_expr=");
            print_num(init_expr);
            print_str(" in upper bits\n");
            
            // Store init_expr in temporal_offset field
            p->nodes[var_node].data.timing.temporal_offset = init_expr;
        }
    }
    
    return var_node;
}

// Parse constant definition: var.c-name-[value]
static uint16_t parse_const_def(Parser* p) {
    Token* const_tok = advance(p); // consume var.c-name- token
    
    
    uint16_t const_node = alloc_node(p, NODE_VAR_DEF); // Reuse VAR_DEF node
    if (const_node == 0) return 0;
    
    // Mark as constant using type 1 in upper 16 bits of temporal_offset
    p->nodes[const_node].data.timing.temporal_offset = ((uint32_t)1 << 16); // 1 = constant
    
    // Extract constant name from the var.c-name- token
    uint32_t name_start = const_tok->start + 6; // Skip "var.c-"
    uint32_t name_len = const_tok->len - 6; // Remove "var.c-"
    
    // Check if it ends with "-" and remove it
    if (name_len > 0 && p->source[const_tok->start + const_tok->len - 1] == '-') {
        name_len--;
    }
    
    // Bounds check
    if (name_len == 0 || name_len > 256) {
        p->has_error = true;
        return 0;
    }
    
    // Store the constant name in string pool
    uint32_t name_offset = p->string_pos;
    
    // Check if we have enough space for the name + null terminator
    if (p->string_pos + name_len + 1 > 4096) {
        p->has_error = true;
        return 0;
    }
    
    for (uint32_t i = 0; i < name_len; i++) {
        p->string_pool[p->string_pos++] = p->source[name_start + i];
    }
    p->string_pool[p->string_pos++] = '\0';
    
    p->nodes[const_node].data.ident.name_offset = name_offset;
    p->nodes[const_node].data.ident.name_len = name_len;
    
    // Check for initializer value in brackets [value]
    if (check(p, TOK_BRACKET_OPEN)) {
        advance(p); // consume [
        
        // Parse the value(s) inside brackets
        uint16_t init_expr = 0;
        if (check(p, TOK_NUMBER)) {
            init_expr = parse_expression(p);
        } else if (check(p, TOK_STRING)) {
            // Handle string values
            Token* str_tok = advance(p);
            uint16_t str_node = alloc_node(p, NODE_STRING);
            if (str_node == 0) return 0;
            
            // Store string content
            uint32_t str_offset = store_string_literal(p, str_tok);
            p->nodes[str_node].data.ident.name_offset = str_offset;
            p->nodes[str_node].data.ident.name_len = str_tok->len - 2; // Remove quotes
            init_expr = str_node;
        }
        
        // Expect closing bracket
        if (!match(p, TOK_BRACKET_CLOSE)) {
            p->has_error = true;
            return 0;
        }
        
        // Pack type and init expression into temporal_offset
        if (init_expr != 0) {
            uint32_t packed = ((uint32_t)1 << 16) | (init_expr & 0xFFFF);
            p->nodes[const_node].data.timing.temporal_offset = packed;
        }
    }
    
    return const_node;
}

// Parse pipe-delimited identifier |name|
static uint16_t parse_pipe_identifier(Parser* p) {
    if (!match(p, TOK_PIPE)) return 0;
    
    // Expect identifier
    if (!check(p, TOK_IDENTIFIER)) {
        p->has_error = true;
        return 0;
    }
    
    Token* name_tok = advance(p);
    
    // Expect closing pipe
    if (!match(p, TOK_PIPE)) {
        p->has_error = true;
        return 0;
    }
    
    // Create identifier node
    uint16_t id_node = alloc_node(p, NODE_IDENTIFIER);
    if (id_node == 0) return 0;
    
    // Store the name
    uint32_t name_offset = store_string(p, name_tok);
    p->nodes[id_node].data.ident.name_offset = name_offset;
    p->nodes[id_node].data.ident.name_len = name_tok->len;
    
    return id_node;
}

// Parse pipe function definition: |name| compute.can/{@param:x}< :> do/ ... backslash
static uint16_t parse_pipe_func_def(Parser* p) {
    print_str("[PARSER] Entering parse_pipe_func_def\n");
    
    // We know we have |identifier| pattern
    advance(p); // consume first |
    
    if (!check(p, TOK_IDENTIFIER)) {
        print_str("[PARSER] Error: Expected identifier after |\n");
        p->has_error = true;
        return 0;
    }
    
    Token* name_tok = advance(p);
    
    if (!match(p, TOK_PIPE)) {
        p->has_error = true;
        return 0;
    }
    
    uint16_t func_node = alloc_node(p, NODE_FUNC_DEF);
    if (func_node == 0) return 0;
    
    // Mark function as declared if we're in a declare block
    // We use temporal_offset field as a flag (1 = declared, 0 = normal)
    if (in_declare_block) {
        p->nodes[func_node].data.timing.temporal_offset = 1;
    } else {
        p->nodes[func_node].data.timing.temporal_offset = 0;
    }
    
    // Store function name
    uint32_t name_offset = store_string(p, name_tok);
    uint16_t name_node = alloc_node(p, NODE_IDENTIFIER);
    p->nodes[name_node].data.ident.name_offset = name_offset;
    p->nodes[name_node].data.ident.name_len = name_tok->len;
    
    // Store name - we need to use a field that doesn't conflict with body or chaining
    // Store name in the upper 16 bits of temporal_offset while preserving the lower bits
    int32_t temp_offset = p->nodes[func_node].data.timing.temporal_offset;
    // Make sure we preserve the temporal offset value (should be 1 for declared functions)
    p->nodes[func_node].data.timing.temporal_offset = (temp_offset & 0xFFFF) | ((uint32_t)name_node << 16);
    
    
    
    // Parse compute.can or method.can syntax
    print_str("[PARSER] Looking for func.can, current token: ");
    if (p->current < p->count) {
        print_num(p->tokens[p->current].type);
    } else {
        print_str("END");
    }
    print_str("\n");
    
    if (check(p, TOK_FUNC_CAN)) {
        print_str("[PARSER] Found TOK_FUNC_CAN\n");
        // The lexer parsed "func.can" as a TOK_FUNC_CAN token
        advance(p); // consume the method token
    } else if (check(p, TOK_IDENTIFIER)) {
        Token* method = peek(p);
        // Check for compute.can or method.can pattern
        if (peek2(p) && peek2(p)->type == TOK_DOT && 
            peek3(p) && peek3(p)->type == TOK_IDENTIFIER) {
            advance(p); // consume 'compute' or 'method'
            advance(p); // consume '.'
            advance(p); // consume 'can'
        }
    }
    
    // Parse parameters /{@param:name}
    
    // The lexer might have already parsed the parameter as TOK_PARAM
    while (check(p, TOK_PARAM)) {
        advance(p);
    }
    
    // Or it might be separate tokens
    while (match(p, TOK_SLASH)) {
        if (check(p, TOK_LBRACE)) {
            advance(p); // consume {
            if (match(p, TOK_AT)) {
                // Expect param:name pattern
                if (check(p, TOK_IDENTIFIER)) {
                    Token* param_tok = advance(p);
                    // Check for :name part
                    if (match(p, TOK_COLON) && check(p, TOK_IDENTIFIER)) {
                        Token* name = advance(p);
                        // TODO: Store parameter properly
                    }
                }
            }
            match(p, TOK_RBRACE); // consume }
        } else if (check(p, TOK_PARAM)) {
            // Handle pre-lexed parameter token
            advance(p);
        }
    }
    
    // Look for < to open function body
    print_str("[PARSER] Looking for < to open function body, current token: ");
    if (p->current < p->count) {
        print_num(p->tokens[p->current].type);
    } else {
        print_str("END");
    }
    print_str("\n");
    
    if (match(p, TOK_LT)) {
        print_str("[PARSER] Found <, parsing action block\n");
        
        // Now parse action block - this is the function body
        uint16_t action = parse_action_block(p);
        print_str("[PARSER] parse_action_block returned: ");
        print_num(action);
        print_str("\n");
        
        if (action == 0 || action == 0xFFFF) {
            print_str("[PARSER] Error: action block failed\n");
            p->has_error = true;
            return 0;
        }
        // Store function body in left_idx to avoid conflict with statement chaining
        p->nodes[func_node].data.binary.left_idx = action;
        
        
        
        
        // NOW look for :> at the END to close the function
        print_str("[PARSER] Looking for :> to close function, current token: ");
        if (p->current < p->count) {
            print_num(p->tokens[p->current].type);
        } else {
            print_str("END");
        }
        print_str("\n");
        
        if (!match(p, TOK_BLOCK_END) && !match(p, TOK_FUNC_CLOSE)) {
            print_str("[PARSER] Error: Expected :> to close function\n");
            p->has_error = true;
            return 0;
        }
        
        print_str("[PARSER] Found :>, function parsing complete\n");
        
    } else {
        p->has_error = true;
        return 0;
    }
    
    
    
    return func_node;
}

// Parse function definition |name| method.can...
static uint16_t parse_func_def(Parser* p) {
    // This is now just a fallback for other function syntaxes
    // The pipe function is handled by parse_pipe_func_def
    return 0;
}

// Parse action block
static uint16_t parse_action_block(Parser* p) {
    print_str("[PARSER] parse_action_block: looking for do/, current token: ");
    if (p->current < p->count) {
        print_num(p->tokens[p->current].type);
    } else {
        print_str("END");
    }
    print_str("\n");
    
    if (!match(p, TOK_ACTION_START)) {
        print_str("[PARSER] parse_action_block: no TOK_ACTION_START\n");
        return 0;
    }
    
    print_str("[PARSER] parse_action_block: matched do/, allocating node\n");
    
    uint16_t action_node = alloc_node(p, NODE_ACTION_BLOCK);
    if (action_node == 0) {
        print_str("[PARSER] parse_action_block: alloc_node failed\n");
        return 0;
    }
    
    print_str("[PARSER] parse_action_block: allocated node ");
    print_num(action_node);
    print_str("\n");
    
    // Parse action items until we hit backslash
    uint16_t first_action = 0;
    uint16_t last_action = 0;
    
    while (!at_end(p) && !check(p, TOK_BACKSLASH) && !check(p, TOK_BLOCK_END)) {
        uint16_t stmt = parse_statement(p);
        
        if (first_action == 0) {
            first_action = stmt;
        }
        
        // Chain actions (store in binary op style)
        if (last_action != 0 && last_action < p->node_capacity) {
            p->nodes[last_action].data.binary.right_idx = stmt;
        }
        last_action = stmt;
        
        // Handle connectors and continue markers
        if (match(p, TOK_CONNECTOR_FWD)) {
            // Forward connector \>| - just continue to next statement
            continue;
        } else if (match(p, TOK_CONNECTOR_BWD)) {
            // Backward connector \<| - just continue for now
            continue;
        }
        
        // Continue marker
        match(p, TOK_SLASH);
    }
    
    // Store first action in action block
    p->nodes[action_node].data.binary.left_idx = first_action;
    
    print_str("[PARSER] parse_action_block: looking for ending backslash, current token: ");
    if (p->current < p->count) {
        print_num(p->tokens[p->current].type);
    } else {
        print_str("END");
    }
    print_str("\n");
    
    if (match(p, TOK_BACKSLASH)) {
        print_str("[PARSER] parse_action_block: consumed ending backslash\n");
    } else {
        print_str("[PARSER] parse_action_block: no ending backslash found\n");
    }
    
    
    return action_node;
}

// Parse declare block
// We don't need this function anymore since we handle declare/ inline

// Parse conditional
static uint16_t parse_conditional(Parser* p) {
    // Advance the conditional token (f.if, f.ens, etc.)
    Token* cond_tok = advance(p);
    
    uint16_t cond_node = alloc_node(p, NODE_CONDITIONAL);
    if (cond_node == 0) return 0;
    
    // Store the conditional type
    p->nodes[cond_node].data.binary.op = cond_tok->type;
    
    // Parse parameter if present /{@param:name} or just /expression
    if (match(p, TOK_SLASH)) {
        // Check for parameter syntax
        if (check(p, TOK_LBRACE) || check(p, TOK_PARAM)) {
            // Skip parameter parsing for now
            while (!at_end(p) && !check(p, TOK_STAR) && !check(p, TOK_GT) && 
                   !check(p, TOK_LT) && !check(p, TOK_EQUALS)) {
                advance(p);
            }
        } else {
            // Parse the condition expression
            uint16_t condition = parse_expression(p);
            p->nodes[cond_node].data.binary.left_idx = condition;
        }
    }
    
    // Handle the comparison and then clause
    uint16_t then_start = 0;
    uint16_t then_end = 0;
    
    // Parse until we hit \>| (end of conditional)
    while (!at_end(p) && !check(p, TOK_BACKSLASH)) {
        if (check(p, TOK_CONNECTOR_FWD)) {
            advance(p); // consume \>|
            break;
        }
        
        // Parse statements in the then clause
        uint16_t stmt = parse_statement(p);
        if (then_start == 0) {
            then_start = stmt;
            p->nodes[cond_node].data.binary.right_idx = then_start;
        }
        
        // Chain statements
        if (then_end != 0 && then_end < p->node_capacity) {
            p->nodes[then_end].data.binary.right_idx = stmt;
        }
        then_end = stmt;
    }
    
    // Consume ending backslash if present
    if (match(p, TOK_BACKSLASH)) {
        // Check for connector after backslash
        if (match(p, TOK_GT) && match(p, TOK_PIPE)) {
            // This is \>| pattern
        }
    }
    
    return cond_node;
}

// Parse statement
static uint16_t parse_statement(Parser* p) {
    if (at_end(p)) return 0;
    
    print_str("[PARSER-STMT] current token type=");
    print_num(p->tokens[p->current].type);
    print_str(" at pos ");
    print_num(p->current);
    print_str("\n");
    
    // Declare block start
    if (check(p, TOK_DECLARE)) {
        advance(p);  // Consume 'declare'
        if (!match(p, TOK_SLASH)) {
            p->has_error = true;
            return 0;
        }
        in_declare_block = true;
        return 0xFFFE;  // Special "skip" marker
    }
    
    // End of declare block
    if (in_declare_block && check(p, TOK_BACKSLASH)) {
        advance(p);  // Consume '\\'
        in_declare_block = false;
        return 0xFFFE;  // Special "skip" marker
    }
    
    // Variable definition (all types)
    if (check(p, TOK_VAR) || check(p, TOK_VAR_INT) || 
        check(p, TOK_VAR_FLOAT) || check(p, TOK_VAR_STRING) || 
        check(p, TOK_VAR_BOOL)) {
        print_str("[PARSER] Parsing variable definition\n");
        uint16_t var_node = parse_var_def(p);
        print_str("[PARSER] parse_var_def returned node_idx=");
        print_num(var_node);
        print_str("\n");
        return var_node;
    }
    
    // Constant definition
    if (check(p, TOK_CONST)) {
        return parse_const_def(p);
    }
    
    // Array 4D definition
    if (check(p, TOK_ARRAY_4D)) {
        return parse_array_4d_def(p);
    }
    
    // Function definition with pipe pattern |name|
    if (check(p, TOK_PIPE)) {
        Token* tok2 = peek2(p);
        Token* tok3 = peek3(p);
        print_str("[PARSER] Checking pipe pattern: tok2=");
        if (tok2) {
            print_num(tok2->type);
            print_str(" tok3=");
            if (tok3) print_num(tok3->type);
            else print_str("NULL");
        } else {
            print_str("NULL");
        }
        print_str("\n");
        if (tok2 && tok3) {
            if (tok2->type == TOK_IDENTIFIER && tok3->type == TOK_PIPE) {
                print_str("[PARSER] Found pipe function definition\n");
                // This is a pipe function definition
                return parse_pipe_func_def(p);
            }
        }
    }
    
    // Action block
    if (check(p, TOK_ACTION_START)) {
        return parse_action_block(p);
    }
    
    // Conditional - check for all conditional tokens
    if (check(p, TOK_FUNC_CAN) || check(p, TOK_COND_IF) || 
        check(p, TOK_COND_ENS) || check(p, TOK_COND_VER) ||
        check(p, TOK_COND_CHK) || check(p, TOK_COND_TRY) ||
        check(p, TOK_COND_GRD) || check(p, TOK_COND_UNL) ||
        check(p, TOK_COND_WHL) || check(p, TOK_COND_UNT)) {
        return parse_conditional(p);
    }
    
    // Jump marker - could be timeline jump or function call
    if (check(p, TOK_JUMP_MARKER)) {
        // Look ahead to distinguish between:
        // - ^timeline.[...]  (timeline jump)
        // - ^function_name/  (function call)
        
        Token* next = peek2(p);
        Token* after = peek3(p);
        
        // Check if it's a timeline pattern
        if (next && next->type == TOK_IDENTIFIER) {
            // Check if the identifier starts with "timeline"
            bool is_timeline = false;
            if (next->len >= 8) {
                const char* timeline_str = "timeline";
                is_timeline = true;
                for (int i = 0; i < 8; i++) {
                    if (p->source[next->start + i] != timeline_str[i]) {
                        is_timeline = false;
                        break;
                    }
                }
            }
            
            if (is_timeline) {
                // This is a timeline jump
                advance(p); // consume ^
                uint16_t jump_node = alloc_node(p, NODE_JUMP);
                if (check(p, TOK_IDENTIFIER)) {
                    Token* target = advance(p);
                    p->nodes[jump_node].data.ident.name_offset = target->start;
                    p->nodes[jump_node].data.ident.name_len = target->len;
                }
                return jump_node;
            }
        }
        
        // Not a timeline - treat as function call
        advance(p); // consume ^
        
        // Next should be an identifier (function name)
        if (!check(p, TOK_IDENTIFIER)) {
            return 0;
        }
        
        Token* name_tok = advance(p);
        if (!name_tok) {
            return 0;
        }
        
        
        uint16_t call_node = alloc_node(p, NODE_FUNC_CALL);
        if (call_node == 0) return 0;
        
        // Create identifier node for the function name
        uint16_t name_node = alloc_node(p, NODE_IDENTIFIER);
        if (name_node == 0) return 0;
        
        // Store name in string pool
        uint32_t name_offset = p->string_pos;
        
        // Check if we have enough space for the name + null terminator
        if (p->string_pos + name_tok->len + 1 > 4096) {
            p->has_error = true;
            return 0;
        }
        
        for (uint32_t i = 0; i < name_tok->len; i++) {
            p->string_pool[p->string_pos++] = p->source[name_tok->start + i];
        }
        p->string_pool[p->string_pos++] = '\0';
        
        p->nodes[name_node].data.ident.name_offset = name_offset;
        p->nodes[name_node].data.ident.name_len = name_tok->len;
        
        // Store function name in left
        p->nodes[call_node].data.binary.left_idx = name_node;
        
        // Parse parameters /{@param:value}
        uint16_t first_param = 0;
        uint16_t last_param = 0;
        
        while (check(p, TOK_SLASH)) {
            advance(p); // consume /
            
            // Look for {@param:value} pattern
            if (check(p, TOK_LBRACE)) {
                advance(p); // consume {
                
                if (match(p, TOK_AT)) {
                    // Expect param:value pattern
                    if (check(p, TOK_IDENTIFIER)) {
                        Token* param_name = advance(p);
                        
                        if (match(p, TOK_COLON)) {
                            // Parse the value (could be number or identifier)
                            uint16_t value_node = 0;
                            
                            if (check(p, TOK_NUMBER)) {
                                value_node = parse_number(p);
                            } else if (check(p, TOK_IDENTIFIER)) {
                                value_node = parse_identifier(p);
                            }
                            
                            if (value_node != 0) {
                                // Create parameter node using EXPRESSION to hold the parameter
                                uint16_t param_node = alloc_node(p, NODE_EXPRESSION);
                                if (param_node != 0) {
                                    p->nodes[param_node].data.binary.left_idx = value_node;
                                    
                                    // Chain parameters
                                    if (first_param == 0) {
                                        first_param = param_node;
                                    }
                                    if (last_param != 0) {
                                        p->nodes[last_param].data.binary.right_idx = param_node;
                                    }
                                    last_param = param_node;
                                }
                            }
                        }
                    }
                }
                match(p, TOK_RBRACE); // consume }
            } else if (check(p, TOK_PARAM)) {
                // Handle pre-lexed parameter token
                advance(p);
            }
        }
        
        // Store parameters in right node
        if (first_param != 0) {
            p->nodes[call_node].data.binary.right_idx = first_param;
        }
        
        // Consume trailing slash if present
        if (check(p, TOK_SLASH)) {
            advance(p);
        }
        
        return call_node;
    }
    
    // Output methods
    if (check(p, TOK_PRINT) || check(p, TOK_TXT) || check(p, TOK_OUT) || 
        check(p, TOK_FMT) || check(p, TOK_DYN)) {
        TokenType output_type = advance(p)->type;
        uint16_t output_node = alloc_node(p, NODE_OUTPUT);
        if (output_node == 0) return 0;
        
        p->nodes[output_node].data.output.output_type = output_type;
        p->nodes[output_node].data.output.next_output = 0xFFFF; // No chaining yet
        
        // Consume the slash after output keyword
        if (check(p, TOK_SLASH)) {
            advance(p);
        } else if (check(p, TOK_DIV)) {
            advance(p);
        }
        
        // Parse content (identifier, number, or string)
        Token* next_tok = peek(p);
        
        if (check(p, TOK_IDENTIFIER) || check(p, TOK_VAR) || 
            check(p, TOK_VAR_INT) || check(p, TOK_VAR_FLOAT) || 
            check(p, TOK_VAR_STRING) || check(p, TOK_VAR_BOOL) || 
            check(p, TOK_CONST)) {
            // Parse identifier or variable reference
            Token* tok = advance(p);
            uint16_t id_node = alloc_node(p, NODE_IDENTIFIER);
            if (id_node == 0) return 0;
            
            // For variable tokens, extract just the name part
            uint32_t name_start = tok->start;
            uint32_t name_len = tok->len;
            
            if (tok->type != TOK_IDENTIFIER) {
                // It's a typed variable token, extract the name
                if (tok->len > 6 && (tok->type == TOK_VAR_INT || tok->type == TOK_VAR_FLOAT ||
                                     tok->type == TOK_VAR_STRING || tok->type == TOK_VAR_BOOL ||
                                     tok->type == TOK_CONST)) {
                    // Skip "var.t-" prefix
                    name_start = tok->start + 6;
                    name_len = tok->len - 6;
                    // Remove trailing dash if present
                    if (name_len > 0 && p->source[tok->start + tok->len - 1] == '-') {
                        name_len--;
                    }
                } else if (tok->len > 4 && tok->type == TOK_VAR) {
                    // Could be var.v- or var.name- syntax
                    if (tok->len > 6 && str_equals(&p->source[tok->start], "var.v-", 6)) {
                        name_start = tok->start + 6;
                        name_len = tok->len - 6;
                        if (name_len > 0 && p->source[tok->start + tok->len - 1] == '-') {
                            name_len--;
                        }
                    } else {
                        // var.name- syntax
                        name_start = tok->start + 4;
                        name_len = 0;
                        for (uint32_t i = 4; i < tok->len; i++) {
                            if (p->source[tok->start + i] == '-') {
                                break;
                            }
                            name_len++;
                        }
                    }
                }
            }
            
            // Store the variable name
            uint32_t name_offset = p->string_pos;
            for (uint32_t i = 0; i < name_len; i++) {
                p->string_pool[p->string_pos++] = p->source[name_start + i];
            }
            p->string_pool[p->string_pos++] = '\0';
            
            p->nodes[id_node].data.ident.name_offset = name_offset;
            p->nodes[id_node].data.ident.name_len = name_len;
            
            p->nodes[output_node].data.output.content_idx = id_node;
        } else if (check(p, TOK_NUMBER) || check(p, TOK_MINUS) || 
                   check(p, TOK_LPAREN) || check(p, TOK_MATH_PREFIX)) {
            // Parse expression (could be number, arithmetic, math function, etc.)
            uint16_t expr_node = parse_expression(p);
            p->nodes[output_node].data.output.content_idx = expr_node;
        } else if (check(p, TOK_STRING)) {
            // Parse string
            Token* str_tok = advance(p);
            
            // Create a NODE_STRING to hold the string literal
            uint16_t str_node = alloc_node(p, NODE_STRING);
            if (str_node == 0) return 0;
            
            uint32_t str_offset = store_string_literal(p, str_tok);
            p->nodes[str_node].data.ident.name_offset = str_offset;
            p->nodes[str_node].data.ident.name_len = str_tok->len - 2; // Exclude quotes
            
            print_str("[PARSER] Created NODE_STRING at idx=");
            print_num(str_node);
            print_str(" for print statement\n");
            
            p->nodes[output_node].data.output.content_idx = str_node;
        } else {
            // No content - set to invalid
            p->nodes[output_node].data.output.content_idx = 0xFFFF;
        }
        
        // Consume ending backslash if present
        if (check(p, TOK_BACKSLASH)) {
            advance(p);
        }
        
        return output_node;
    }
    
    // Inline assembly
    if (match(p, TOK_ASM)) {
        uint16_t asm_node = alloc_node(p, NODE_INLINE_ASM);
        if (asm_node == 0) return 0;
        
        // Expect assembly code as string or raw text
        if (check(p, TOK_STRING)) {
            Token* asm_tok = advance(p);
            uint32_t asm_offset = store_string_literal(p, asm_tok);
            p->nodes[asm_node].data.inline_asm.code_offset = asm_offset;
            // Store actual content length (without quotes)
            p->nodes[asm_node].data.inline_asm.code_len = asm_tok->len - 2;
        }
        
        return asm_node;
    }
    
    // Check for simple variable definition syntax: v/ name value
    if (check(p, TOK_IDENTIFIER) && peek(p)->len == 1) {
        Token* id_tok = peek(p);
        if (p->source[id_tok->start] == 'v' && peek2(p) && peek2(p)->type == TOK_SLASH) {
            // This is a variable definition with v/ syntax
            advance(p); // consume 'v'
            advance(p); // consume '/'
            
            // Get variable name
            if (!check(p, TOK_IDENTIFIER)) {
                return 0; // Error: expected variable name
            }
            Token* name_tok = advance(p);
            
            // Create variable definition node
            uint16_t var_node = alloc_node(p, NODE_VAR_DEF);
            if (var_node == 0) return 0;
            
            // Copy variable name to string pool
            uint32_t name_offset = p->string_pos;
            for (uint32_t i = 0; i < name_tok->len; i++) {
                p->string_pool[p->string_pos++] = p->source[name_tok->start + i];
            }
            p->string_pool[p->string_pos++] = '\0'; // Add null terminator
            
            // Initialize value (if provided)
            uint16_t init_idx = 0;
            if (!at_end(p) && !check(p, TOK_BACKSLASH)) {
                // Parse initialization value
                init_idx = parse_expression(p);
            }
            
            // Pack data into name_len field:
            // - bits 0-15: name length
            // - bits 16-23: init_idx (limited to 255)
            // - bits 24-31: var_type (v = 'v' for generic var)
            uint32_t packed_data = (name_tok->len & 0xFFFF) | 
                                  ((init_idx & 0xFF) << 16) | 
                                  ('v' << 24);
            
            p->nodes[var_node].data.ident.name_offset = name_offset;
            p->nodes[var_node].data.ident.name_len = packed_data;
            
            return var_node;
        }
    }
    
    // Check for standalone backslash
    if (check(p, TOK_BACKSLASH)) {
        print_str("[PARSER-STMT] Skipping standalone backslash\n");
        advance(p);
        return 0xFFFF;
    }
    
    // Expression statement
    uint16_t expr = parse_expression(p);
    
    // If expression failed and we haven't advanced, skip this token to avoid infinite loop
    if (expr == 0xFFFF || expr == 0) {
        Token* current_tok = peek(p);
        if (current_tok && current_tok->type != TOK_EOF) {
            advance(p); // Skip the problematic token
        }
        return 0xFFFF;
    }
    
    return expr;
}

// Main parse function
// Static parser in BSS - guaranteed zero-initialized
static Parser parser = {0};
static uint64_t parser_canary = 0xCAFEBABECAFEBABEULL;

// Parser initialization helper
static inline void parser_init(Token* tokens, uint32_t count, ASTNode* node_pool,
                               uint32_t pool_size, char* string_pool, const char* source) {
    // Clear everything first
    __builtin_memset(&parser, 0, sizeof(Parser));
    
    // Set fields
    parser.tokens = tokens;
    parser.count = count;
    parser.current = 0;
    parser.nodes = node_pool;
    parser.node_count = 1;  // Start at 1 to avoid 0 being treated as error
    parser.node_capacity = pool_size;
    parser.string_pool = string_pool;
    parser.string_pos = 0;
    parser.source = source;
    parser.has_error = false;
    parser.error_pos = 0;
    
    // Verify canary is intact
    if (parser_canary != 0xCAFEBABECAFEBABEULL) {
        print_str("[PARSER] FATAL: Parser canary corrupted before init!\n");
        __builtin_trap();
    }
}

uint16_t parse_blaze(Token* tokens, uint32_t count, ASTNode* node_pool, 
                     uint32_t pool_size, char* string_pool, const char* source) {
    print_str("[PARSER] parse_blaze called with count=");
    print_num(count);
    print_str(" pool_size=");
    print_num(pool_size);
    print_str(" nodes addr=");
    print_num((uint64_t)node_pool);
    print_str("\n");
    
    // Initialize the static parser
    parser_init(tokens, count, node_pool, pool_size, string_pool, source);
    
    print_str("[PARSER] Initialized parser with node_count=");
    print_num(parser.node_count);
    print_str(" node_capacity=");
    print_num(parser.node_capacity);
    print_str("\n");
    
    // Create root program node
    uint16_t program_node = alloc_node(&parser, NODE_PROGRAM);
    if (program_node == 0) {
        print_str("[PARSER] Failed to allocate program node\n");
        return 0;
    }
    
    print_str("[PARSER] Created program node at idx=");
    print_num(program_node);
    print_str(" type=");
    print_num(parser.nodes[program_node].type);
    print_str("\n");
    
    // Parse all statements
    volatile uint16_t first_stmt = 0;
    volatile uint16_t last_stmt = 0;
    
    while (!at_end(&parser)) {
        print_str("[PARSER] Loop iteration: current=");
        print_num(parser.current);
        print_str(" count=");
        print_num(parser.count);
        print_str("\n");
        
        volatile uint16_t stmt = parse_statement(&parser);
        __asm__ volatile("" ::: "memory");  // Memory barrier
        print_str("[PARSER] parse_statement returned ");
        print_num(stmt);
        print_str("\n");
        
        if (parser.has_error) {
            print_str("[PARSER] Returning 0 due to parser.has_error\n");
            return 0; // Parse failed
        }
        
        // Skip if no valid statement (e.g., comments only)
        if (stmt == 0 || stmt == 0xFFFF) {
            print_str("[PARSER] Skipping invalid statement\n");
            continue;
        }
        
        // Skip declare markers
        if (stmt == 0xFFFE) {
        // print_str("  Skipping declare marker\n");
            continue;
        }
        
        print_str("[PARSER] Got statement idx=");
        print_num(stmt);
        if (stmt > 0 && stmt < parser.node_capacity) {
            print_str(" type=");
            print_num(parser.nodes[stmt].type);
        }
        print_str("\n");
        
        if (first_stmt == 0) {
            first_stmt = stmt;
            parser.nodes[program_node].data.binary.left_idx = first_stmt;
            print_str("[PARSER] Set first_stmt=");
            print_num(first_stmt);
            print_str("\n");
        }
        
        // Chain statements
        if (last_stmt != 0 && last_stmt < pool_size) {
            print_str("[PARSER] Chaining stmt=");
            print_num(stmt);
            print_str(" to last_stmt=");
            print_num(last_stmt);
            print_str("\n");
            parser.nodes[last_stmt].data.binary.right_idx = stmt;
        }
        last_stmt = stmt;
        
        print_str("[PARSER] Updated last_stmt=");
        print_num(last_stmt);
        print_str(" at_end=");
        print_num(at_end(&parser));
        print_str("\n");
    }
    
    print_str("[PARSER] Exited main loop\n");
    
    // Final canary check
    if (parser_canary != 0xCAFEBABECAFEBABEULL) {
        print_str("[PARSER] FATAL: Parser canary corrupted at end! Value=");
        print_num(parser_canary);
        print_str("\n");
        __builtin_trap();
    }
    
    print_str("[PARSER] End of parsing: program_node=");
    print_num(program_node);
    print_str(" parser.has_error=");
    print_num(parser.has_error);
    print_str(" current=");
    print_num(parser.current);
    print_str(" count=");
    print_num(parser.count);
    print_str("\n");
    
    if (parser.has_error) {
        print_str("[PARSER] Returning 0 due to error\n");
        return 0;
    }
    
    print_str("[PARSER] Returning program_node=");
    print_num(program_node);
    print_str("\n");
    return program_node;
}