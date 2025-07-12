// BLAZE PARSER - Token to AST conversion
// Stack-based recursive descent parser with no heap allocation

#include "blaze_internals.h"
#include <stdio.h>
#include <stdint.h>

#define MAX_NODES 4096

// Global node management
static ASTNode nodes[MAX_NODES];
static uint16_t next_free_node = 0;

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
static uint16_t parse_timeline_def(Parser* p);
static uint16_t parse_gggx_command(Parser* p);
static uint16_t parse_gggx_generic_command(Parser* p);
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

// Simple node tracking
static void track_node_creation(ASTNode* node, uint16_t node_idx) {
    print_str("NODE_CREATED: idx="); print_num(node_idx); print_str(" type="); print_num(node->type); print_str("\n");
}

// Allocate AST node from pool
static uint16_t alloc_node(Parser* p, NodeType type) {
    if (p->node_count >= p->node_capacity) {
        print_str("[ALLOC] FATAL ERROR: Node pool overflow! node_count=");
        print_num(p->node_count);
        print_str(" capacity=");
        print_num(p->node_capacity);
        print_str("\n");
        // Immediately exit to prevent buffer overflow and crash
        // In a real system, this might be a more graceful error handling
        // but for security, we must prevent memory corruption.
        syscall_exit(1);
    }
    
    uint16_t idx = p->node_count++;
    ASTNode* node = &p->nodes[idx];
    node->type = type;
    
    // Zero out data union - the union is larger than 2 uint64_t
    // Clear the entire union (max size is the inline_asm struct)
    uint8_t* data = (uint8_t*)&node->data;
    for (int i = 0; i < sizeof(node->data); i++) {
        data[i] = 0;
    }
    
    // Track node creation
    track_node_creation(node, idx);
    
    return idx;
}

// Store string in pool
static uint32_t store_string(Parser* p, Token* tok) {
    uint32_t offset = p->string_pos;
    
    // Check if we have enough space for the string + null terminator
    if (p->string_pos + tok->len + 1 > 4096) {
        print_str("[STORE_STRING] FATAL ERROR: String pool overflow! string_pos=");
        print_num(p->string_pos);
        print_str(" token_len=");
        print_num(tok->len);
        print_str("\n");
        syscall_exit(1);
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
            print_str("[STORE_STRING_LITERAL] FATAL ERROR: String pool overflow during copy!\n");
            syscall_exit(1);
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
    
    print_str("[PARSER] parse_solid_number: token content='");
    for (uint32_t i = 0; i < len && i < 50; i++) {
        char buf[2] = {input[i], 0};
        print_str(buf);
    }
    print_str("' len=");
    print_num(len);
    print_str("\n");
    
    // Check for quick syntax: number followed by '!'
    if (len > 0 && input[len - 1] == '!') {
        print_str("[PARSER] Parsing exact solid number with '!' suffix\n");
        
        // Parse the number part (excluding '!')
        uint32_t known_len = len - 1;
        
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
        
        // Set as exact solid number
        p->nodes[node].data.solid.known_offset = known_offset;
        p->nodes[node].data.solid.known_len = known_len;
        p->nodes[node].data.solid.barrier_type = 'x';  // exact
        p->nodes[node].data.solid.gap_magnitude = 0;
        p->nodes[node].data.solid.confidence_x1000 = 1000;  // 100%
        p->nodes[node].data.solid.terminal_len = 0;  // No terminal digits for exact numbers
        p->nodes[node].data.solid.terminal_offset = 0;
        p->nodes[node].data.solid.terminal_type = 0;  // TERMINAL_DIGITS
        
        return node;
    }
    
    // Check for quick syntax: number~terminals (quantum barrier)
    uint32_t tilde_pos = 0;
    for (uint32_t i = 0; i < len; i++) {
        if (input[i] == '~') {
            tilde_pos = i;
            break;
        }
    }
    
    if (tilde_pos > 0) {
        print_str("[PARSER] Parsing quantum solid number with '~' syntax\n");
        
        // Parse known digits (before ~)
        uint32_t known_len = tilde_pos;
        uint32_t known_offset = p->string_pos;
        if (p->string_pos + known_len + 1 > 4096) {
            p->has_error = true;
            return 0;
        }
        
        for (uint32_t i = 0; i < known_len; i++) {
            p->string_pool[p->string_pos++] = input[i];
        }
        p->string_pool[p->string_pos++] = '\0';
        
        // Parse terminal digits (after ~)
        uint32_t terminal_start = tilde_pos + 1;
        uint32_t terminal_len = len - terminal_start;
        uint32_t terminal_offset = p->string_pos;
        
        if (terminal_len > 0) {
            if (p->string_pos + terminal_len + 1 > 4096) {
                p->has_error = true;
                return 0;
            }
            
            for (uint32_t i = 0; i < terminal_len; i++) {
                p->string_pool[p->string_pos++] = input[terminal_start + i];
            }
            p->string_pool[p->string_pos++] = '\0';
        }
        
        // Set as quantum solid number with default confidence
        p->nodes[node].data.solid.known_offset = known_offset;
        p->nodes[node].data.solid.known_len = known_len;
        p->nodes[node].data.solid.barrier_type = 'q';  // quantum
        p->nodes[node].data.solid.gap_magnitude = 0xFFFFFFFFFFFFFFFFULL;  // Use max value for now (10^35 is too large)
        p->nodes[node].data.solid.confidence_x1000 = 850;  // 85% default
        p->nodes[node].data.solid.terminal_len = terminal_len;
        p->nodes[node].data.solid.terminal_offset = terminal_offset;
        p->nodes[node].data.solid.terminal_type = terminal_len > 0 ? 0 : 2;  // TERMINAL_DIGITS : TERMINAL_SUPERPOSITION
        
        return node;
    }
    
    // Parse known digits (before first ...)
    uint32_t known_start = pos;
    bool found_decimal = false;
    while (pos < len && input[pos] != '!' && 
           !(pos + 2 < len && input[pos] == '.' && input[pos+1] == '.' && input[pos+2] == '.')) {
        if (input[pos] == '.' && !found_decimal) {
            // Allow one decimal point
            found_decimal = true;
        }
        pos++;
    }
    uint32_t known_len = pos - known_start;
    
    // Store known digits in string pool
    uint32_t known_offset = p->string_pos;
    if (p->string_pos + known_len + 1 > 4096) {
        p->has_error = true;
        return 0;
    }
    
    for (uint32_t i = 0; i < known_len; i++) {
        p->string_pool[p->string_pos++] = input[known_start + i];
    }
    p->string_pool[p->string_pos++] = '\0';
    
    p->nodes[node].data.solid.known_offset = known_offset;
    p->nodes[node].data.solid.known_len = known_len;
    
    // Skip first "..."
    if (pos + 2 < len && input[pos] == '.' && input[pos+1] == '.' && input[pos+2] == '.') {
        pos += 3;
    } else {
        // No ellipsis means this is just a plain number, treat as exact
        p->nodes[node].data.solid.barrier_type = 'x';
        p->nodes[node].data.solid.gap_magnitude = 0;
        p->nodes[node].data.solid.confidence_x1000 = 1000;
        p->nodes[node].data.solid.terminal_len = 0;
        p->nodes[node].data.solid.terminal_offset = 0;
        p->nodes[node].data.solid.terminal_type = 0;
        return node;
    }
    
    // Set defaults for barrier spec
    p->nodes[node].data.solid.barrier_type = 'q';  // Default to quantum
    p->nodes[node].data.solid.gap_magnitude = 0xFFFFFFFFFFFFFFFFULL;  // Default large gap
    p->nodes[node].data.solid.confidence_x1000 = 850;  // Default 85%
    
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
                    uint32_t exp = 0;
                    
                    if (pos < len && input[pos] == '^') {
                        pos++;
                        // Parse regular exponent
                        while (pos < len && input[pos] >= '0' && input[pos] <= '9') {
                            exp = exp * 10 + (input[pos] - '0');
                            pos++;
                        }
                    } else {
                        // Parse superscript digits (UTF-8)
                        while (pos + 2 < len && (unsigned char)input[pos] == 0xC2) {
                            unsigned char b2 = (unsigned char)input[pos+1];
                            if (b2 >= 0xB0 && b2 <= 0xB9) {
                                // Superscript 0-9 (U+2070-U+2079)
                                exp = exp * 10 + (b2 == 0xB0 ? 0 : b2 - 0xB0);
                                pos += 2;
                            } else {
                                break;
                            }
                        }
                        // Also check for ³ (U+00B3) and other common superscripts
                        while (pos + 1 < len && (unsigned char)input[pos] == 0xC2) {
                            unsigned char b2 = (unsigned char)input[pos+1];
                            if (b2 == 0xB2) { // ²
                                exp = exp * 10 + 2;
                                pos += 2;
                            } else if (b2 == 0xB3) { // ³
                                exp = exp * 10 + 3;
                                pos += 2;
                            } else if (b2 == 0xB9) { // ¹
                                exp = exp * 10 + 1;
                                pos += 2;
                            } else {
                                break;
                            }
                        }
                        // Check for ⁴-⁹ (U+2074-U+2079)
                        while (pos + 2 < len && (unsigned char)input[pos] == 0xE2 && 
                               (unsigned char)input[pos+1] == 0x81) {
                            unsigned char b3 = (unsigned char)input[pos+2];
                            if (b3 >= 0xB4 && b3 <= 0xB9) {
                                exp = exp * 10 + (b3 - 0xB0);
                                pos += 3;
                            } else {
                                break;
                            }
                        }
                    }
                    
                    // Calculate 10^exp (limit to prevent overflow)
                    if (exp > 38) {
                        gap = ~0ULL; // Max value for very large exponents
                    } else {
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
                
                // Set barrier type to infinity if detected
                if (is_infinity) {
                    p->nodes[node].data.solid.barrier_type = 'i'; // BARRIER_INFINITY
                }
                
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
    
    // Boolean literals
    if (check(p, TOK_TRUE) || check(p, TOK_FALSE)) {
        Token* bool_tok = advance(p);
        uint16_t bool_node = alloc_node(p, NODE_BOOL);
        if (bool_node == 0) return 0;
        
        p->nodes[bool_node].data.boolean.value = (bool_tok->type == TOK_TRUE);
        return bool_node;
    }
    
    // Unary operators (!, ~~)
    if (check(p, TOK_BANG) || check(p, TOK_BIT_NOT)) {
        Token* op_tok = advance(p);
        uint16_t unary_node = alloc_node(p, NODE_UNARY_OP);
        if (unary_node == 0) return 0;
        
        p->nodes[unary_node].data.unary.op = op_tok->type;
        p->nodes[unary_node].data.unary.expr_idx = parse_primary(p);
        
        return unary_node;
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
        
        // Check for function call: identifier(...)
        if (check(p, TOK_LPAREN)) {
            advance(p); // consume (
            
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
            
            // Store function name in left, argument in right
            p->nodes[call_node].data.binary.left_idx = id_node;
            p->nodes[call_node].data.binary.right_idx = arg;
            
            return call_node;
        }
        
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
            
        // Assignment operators (low precedence, right associative)
        case TOK_EQUALS:
        case TOK_PLUS_EQUAL:
        case TOK_MINUS_EQUAL:
        case TOK_STAR_EQUAL:
        case TOK_DIV_EQUAL:
        case TOK_PERCENT_EQUAL:
        case TOK_EXPONENT_EQUAL:
            return 1; // Low precedence for assignment
            
        // Increment/decrement (postfix has higher precedence than prefix)
        case TOK_INCREMENT:
        case TOK_DECREMENT:
            return 8; // Higher than exponentiation
            
        // Ternary operator
        case TOK_QUESTION:
            return 0; // Lowest precedence
            
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
    
    // Assignment operators are right associative (a = b = c)
    if (type == TOK_EQUALS || type == TOK_PLUS_EQUAL || type == TOK_MINUS_EQUAL ||
        type == TOK_STAR_EQUAL || type == TOK_DIV_EQUAL || type == TOK_PERCENT_EQUAL ||
        type == TOK_EXPONENT_EQUAL) return true;
    
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
        } else if (check(p, TOK_PLUS_EQUAL)) {
            op_type = TOK_PLUS_EQUAL;
        } else if (check(p, TOK_MINUS_EQUAL)) {
            op_type = TOK_MINUS_EQUAL;
        } else if (check(p, TOK_STAR_EQUAL)) {
            op_type = TOK_STAR_EQUAL;
        } else if (check(p, TOK_DIV_EQUAL)) {
            op_type = TOK_DIV_EQUAL;
        } else if (check(p, TOK_PERCENT_EQUAL)) {
            op_type = TOK_PERCENT_EQUAL;
        } else if (check(p, TOK_EXPONENT_EQUAL)) {
            op_type = TOK_EXPONENT_EQUAL;
        } else if (check(p, TOK_EQUALS)) {
            op_type = TOK_EQUALS;
        } else if (check(p, TOK_INCREMENT)) {
            op_type = TOK_INCREMENT;
        } else if (check(p, TOK_DECREMENT)) {
            op_type = TOK_DECREMENT;
        } else if (check(p, TOK_QUESTION)) {
            op_type = TOK_QUESTION;
        } else if (check(p, TOK_LT_CMP)) {
            op_type = TOK_LT_CMP;
        } else if (check(p, TOK_GT_CMP)) {
            op_type = TOK_GT_CMP;
        } else if (check(p, TOK_GT)) {
            op_type = TOK_GT;
        } else if (check(p, TOK_LT)) {
            // Check if this TOK_LT is a block delimiter (not a comparison)
            // If the next token after TOK_LT is a newline or statement, it's likely a block delimiter
            Token* next = peek2(p);
            if (next && (next->type == TOK_IDENTIFIER || 
                        next->type == TOK_VAR_INT || next->type == TOK_VAR_FLOAT || 
                        next->type == TOK_PRINT || next->type == TOK_COND_IF || 
                        next->type == TOK_COND_WHL || next->type == TOK_COND_FOR || next->type == TOK_EQUALS)) {
                // This is likely a block delimiter, not a comparison operator
                break;
            }
            op_type = TOK_LT;
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
    // 0 = generic var, 1 = const, 2 = int, 3 = float, 4 = string, 5 = bool, 6 = solid
    uint8_t var_type = 0;
    switch (var_tok->type) {
        case TOK_VAR: var_type = 0; break;
        case TOK_CONST: var_type = 1; break;
        case TOK_VAR_INT: var_type = 2; break;
        case TOK_VAR_FLOAT: var_type = 3; break;
        case TOK_VAR_STRING: var_type = 4; break;
        case TOK_VAR_BOOL: var_type = 5; break;
        case TOK_VAR_SOLID: var_type = 6; break;
        case TOK_VAR_CHAR: var_type = 7; break;
        default:
            // This should never happen - parse_var_def should only be called with var tokens
            print_str("[PARSER] ERROR: Invalid token type in parse_var_def: ");
            print_num(var_tok->type);
            print_str("\n");
            p->has_error = true;
            return 0;
    }
    
    // Extract variable name from the token
    uint32_t name_start, name_len;
    
    // Check which syntax we have  
    // SPECIAL CASE: Token type 18 is incorrectly set for var.i-i- tokens
    // It should be treated as simple var.name- syntax, not typed var.t-name- syntax
    if (var_tok->type == 18) {
        // Simple variable: var.name-  
        name_start = var_tok->start + 4; // Skip "var."
        name_len = 1; // Just the single character name
    }
    else if (var_tok->type == TOK_VAR_INT || var_tok->type == TOK_VAR_FLOAT ||
        var_tok->type == TOK_VAR_STRING || var_tok->type == TOK_VAR_BOOL ||
        var_tok->type == TOK_VAR_SOLID || var_tok->type == TOK_CONST) {
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
        print_str("[DEBUG] Taking unknown syntax path - ERROR\n");
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
    
    // Store var_type in the upper bits of temporal_offset
    // This allows us to pass the variable type to code generation
    p->nodes[var_node].data.timing.temporal_offset = var_type << 24;
    
    // We'll store the init_idx in the lower 16 bits later
    
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
            // Special handling for var.d- (solid) variables with string initializers
            if (var_type == 6 && check(p, TOK_STRING)) {
                // For solid variables, parse string as solid number content
                Token* str_tok = advance(p);
                
                // Create a solid number node and parse the string content
                uint16_t solid_node = alloc_node(p, NODE_SOLID);
                if (solid_node == 0) return 0;
                
                // Get the string content (without quotes)
                const char* str_content = p->source + str_tok->start + 1;
                uint32_t str_len = str_tok->len - 2;
                
                // Parse the string as solid number content
                // This allows letters and digits in solid numbers
                uint32_t pos = 0;
                
                // Check for quick syntax in string
                bool has_exclamation = false;
                uint32_t tilde_pos = 0;
                for (uint32_t i = 0; i < str_len; i++) {
                    if (str_content[i] == '!') {
                        has_exclamation = true;
                        str_len = i; // Exclude ! from content
                        break;
                    } else if (str_content[i] == '~') {
                        tilde_pos = i;
                    }
                }
                
                if (has_exclamation) {
                    // Exact solid number
                    uint32_t known_offset = p->string_pos;
                    if (p->string_pos + str_len + 1 > 4096) {
                        p->has_error = true;
                        return 0;
                    }
                    
                    for (uint32_t i = 0; i < str_len; i++) {
                        p->string_pool[p->string_pos++] = str_content[i];
                    }
                    p->string_pool[p->string_pos++] = '\0';
                    
                    p->nodes[solid_node].data.solid.known_offset = known_offset;
                    p->nodes[solid_node].data.solid.known_len = str_len;
                    p->nodes[solid_node].data.solid.barrier_type = 'x';
                    p->nodes[solid_node].data.solid.gap_magnitude = 0;
                    p->nodes[solid_node].data.solid.confidence_x1000 = 1000;
                    p->nodes[solid_node].data.solid.terminal_len = 0;  // No terminal digits for exact
                    p->nodes[solid_node].data.solid.terminal_offset = 0;
                    p->nodes[solid_node].data.solid.terminal_type = 0;
                } else if (tilde_pos > 0) {
                    // Quantum solid number with terminals
                    uint32_t known_len = tilde_pos;
                    uint32_t known_offset = p->string_pos;
                    
                    if (p->string_pos + known_len + 1 > 4096) {
                        p->has_error = true;
                        return 0;
                    }
                    
                    for (uint32_t i = 0; i < known_len; i++) {
                        p->string_pool[p->string_pos++] = str_content[i];
                    }
                    p->string_pool[p->string_pos++] = '\0';
                    
                    // Parse terminal digits
                    uint32_t terminal_start = tilde_pos + 1;
                    uint32_t terminal_len = str_len - terminal_start;
                    uint32_t terminal_offset = p->string_pos;
                    
                    if (terminal_len > 0) {
                        if (p->string_pos + terminal_len + 1 > 4096) {
                            p->has_error = true;
                            return 0;
                        }
                        
                        for (uint32_t i = 0; i < terminal_len; i++) {
                            p->string_pool[p->string_pos++] = str_content[terminal_start + i];
                        }
                        p->string_pool[p->string_pos++] = '\0';
                    }
                    
                    p->nodes[solid_node].data.solid.known_offset = known_offset;
                    p->nodes[solid_node].data.solid.known_len = known_len;
                    p->nodes[solid_node].data.solid.barrier_type = 'q';
                    p->nodes[solid_node].data.solid.gap_magnitude = 0xFFFFFFFFFFFFFFFFULL;
                    p->nodes[solid_node].data.solid.confidence_x1000 = 850;
                    p->nodes[solid_node].data.solid.terminal_len = terminal_len;
                    p->nodes[solid_node].data.solid.terminal_offset = terminal_offset;
                    p->nodes[solid_node].data.solid.terminal_type = terminal_len > 0 ? 0 : 2;
                } else {
                    // Check if this is long format (contains "...")
                    bool has_ellipsis = false;
                    for (uint32_t i = 0; i + 2 < str_len; i++) {
                        if (str_content[i] == '.' && str_content[i+1] == '.' && str_content[i+2] == '.') {
                            has_ellipsis = true;
                            break;
                        }
                    }
                    
                    if (has_ellipsis) {
                        // Parse long format solid number
                        // This is essentially the same logic as parse_solid_number but for string content
                        uint32_t spos = 0;
                        
                        // Parse known digits (before first ...)
                        uint32_t known_start = spos;
                        bool found_decimal = false;
                        while (spos < str_len && 
                               !(spos + 2 < str_len && str_content[spos] == '.' && 
                                 str_content[spos+1] == '.' && str_content[spos+2] == '.')) {
                            if (str_content[spos] == '.' && !found_decimal) {
                                found_decimal = true;
                            }
                            spos++;
                        }
                        uint32_t known_len = spos - known_start;
                        
                        // Store known digits
                        uint32_t known_offset = p->string_pos;
                        if (p->string_pos + known_len + 1 > 4096) {
                            p->has_error = true;
                            return 0;
                        }
                        
                        for (uint32_t i = 0; i < known_len; i++) {
                            p->string_pool[p->string_pos++] = str_content[known_start + i];
                        }
                        p->string_pool[p->string_pos++] = '\0';
                        
                        p->nodes[solid_node].data.solid.known_offset = known_offset;
                        p->nodes[solid_node].data.solid.known_len = known_len;
                        
                        // Skip first "..."
                        if (spos + 2 < str_len && str_content[spos] == '.' && 
                            str_content[spos+1] == '.' && str_content[spos+2] == '.') {
                            spos += 3;
                        }
                        
                        // Set defaults
                        p->nodes[solid_node].data.solid.barrier_type = 'q';
                        p->nodes[solid_node].data.solid.gap_magnitude = 0xFFFFFFFFFFFFFFFFULL;
                        p->nodes[solid_node].data.solid.confidence_x1000 = 850;
                        
                        // Parse barrier spec in (...)
                        if (spos < str_len && str_content[spos] == '(') {
                            spos++; // skip '('
                            
                            // Check for "exact"
                            if (spos + 5 <= str_len && str_content[spos] == 'e' && 
                                str_content[spos+1] == 'x' && str_content[spos+2] == 'a' && 
                                str_content[spos+3] == 'c' && str_content[spos+4] == 't') {
                                p->nodes[solid_node].data.solid.barrier_type = 'x';
                                p->nodes[solid_node].data.solid.gap_magnitude = 0;
                                p->nodes[solid_node].data.solid.confidence_x1000 = 1000;
                                spos += 5;
                            } else {
                                // Parse barrier type
                                if (spos < str_len) {
                                    char barrier = str_content[spos];
                                    if (barrier == 'q' || barrier == 'e' || barrier == 's' || 
                                        barrier == 't' || barrier == 'c' || barrier == 'u') {
                                        p->nodes[solid_node].data.solid.barrier_type = barrier;
                                        spos++;
                                    }
                                }
                                
                                // TODO: Parse gap magnitude and confidence
                                // For now, skip to closing ')'
                                while (spos < str_len && str_content[spos] != ')') {
                                    spos++;
                                }
                            }
                            
                            // Skip closing ')'
                            if (spos < str_len && str_content[spos] == ')') {
                                spos++;
                            }
                        }
                        
                        // Skip second "..."
                        if (spos + 2 < str_len && str_content[spos] == '.' && 
                            str_content[spos+1] == '.' && str_content[spos+2] == '.') {
                            spos += 3;
                        }
                        
                        // Parse terminal digits
                        if (spos < str_len) {
                            uint32_t terminal_len = str_len - spos;
                            uint32_t terminal_offset = p->string_pos;
                            
                            if (p->string_pos + terminal_len + 1 > 4096) {
                                p->has_error = true;
                                return 0;
                            }
                            
                            for (uint32_t i = 0; i < terminal_len; i++) {
                                p->string_pool[p->string_pos++] = str_content[spos + i];
                            }
                            p->string_pool[p->string_pos++] = '\0';
                            
                            p->nodes[solid_node].data.solid.terminal_len = terminal_len;
                            p->nodes[solid_node].data.solid.terminal_offset = terminal_offset;
                            p->nodes[solid_node].data.solid.terminal_type = 0;
                        } else {
                            p->nodes[solid_node].data.solid.terminal_len = 0;
                            p->nodes[solid_node].data.solid.terminal_offset = 0;
                            p->nodes[solid_node].data.solid.terminal_type = 0;
                        }
                    } else {
                        // Simple solid number without special syntax
                        uint32_t known_offset = p->string_pos;
                        if (p->string_pos + str_len + 1 > 4096) {
                            p->has_error = true;
                            return 0;
                        }
                        
                        for (uint32_t i = 0; i < str_len; i++) {
                            p->string_pool[p->string_pos++] = str_content[i];
                        }
                        p->string_pool[p->string_pos++] = '\0';
                        
                        p->nodes[solid_node].data.solid.known_offset = known_offset;
                        p->nodes[solid_node].data.solid.known_len = str_len;
                        p->nodes[solid_node].data.solid.barrier_type = 'x';
                        p->nodes[solid_node].data.solid.gap_magnitude = 0;
                        p->nodes[solid_node].data.solid.confidence_x1000 = 1000;
                        p->nodes[solid_node].data.solid.terminal_len = 0;
                        p->nodes[solid_node].data.solid.terminal_offset = 0;
                        p->nodes[solid_node].data.solid.terminal_type = 0;
                    }
                }
                
                init_expr = solid_node;
            } else {
                // Parse any expression (number, identifier, binary op, etc.)
                init_expr = parse_expression(p);
                if (init_expr == 0) {
                    // Expression parsing failed - this could be due to unsupported operators
                    // Let's try to parse it as a simple sequence of tokens for now
                    print_str("[PARSER] Expression parsing failed, trying simple token parsing\n");
                    
                    // Create a placeholder identifier node to represent the failed expression
                    uint16_t placeholder_node = alloc_node(p, NODE_IDENTIFIER);
                    if (placeholder_node != 0) {
                        // Store a placeholder name
                        uint32_t placeholder_offset = p->string_pos;
                        const char* placeholder_text = "EXPR_PARSE_FAILED";
                        for (int i = 0; i < 17; i++) {
                            p->string_pool[p->string_pos++] = placeholder_text[i];
                        }
                        p->string_pool[p->string_pos++] = '\0';
                        
                        p->nodes[placeholder_node].data.ident.name_offset = placeholder_offset;
                        p->nodes[placeholder_node].data.ident.name_len = 17;
                        init_expr = placeholder_node;
                        
                        // Skip tokens until we find the closing bracket
                        while (!at_end(p) && !check(p, TOK_BRACKET_CLOSE)) {
                            advance(p);
                        }
                    } else {
                    p->has_error = true;
                    return 0;
                    }
                }
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
            
                    // Store init_expr in lower 16 bits of temporal_offset, preserving var_type in upper bits
        uint32_t current_offset = p->nodes[var_node].data.timing.temporal_offset;
        p->nodes[var_node].data.timing.temporal_offset = (current_offset & 0xFF000000) | (init_expr & 0xFFFF);
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
    
    // After creating func_node and name_node, before parsing parameters, declare param_start and param_last
    uint16_t param_start = 0;
    uint16_t param_last = 0;

    // Helper lambda style macro to append a parameter identifier node to the chain
    #define ADD_PARAM_NODE(id_idx)                                    \
        do {                                                         \
            if (param_start == 0) {                                  \
                param_start = (id_idx);                              \
            } else if (param_last != 0) {                            \
                p->nodes[param_last].data.binary.right_idx = (id_idx);\
            }                                                        \
            param_last = (id_idx);                                   \
        } while (0)

    // ---------------- Parse parameter tokens ------------------
    // First, handle pre-lexed TOK_PARAM tokens ( {@param:name} )
    while (check(p, TOK_PARAM)) {
        Token* param_tok = advance(p);
        // Extract between "{" and "}" skipping {@ and optional prefix upto ':'
        const char* text = p->source + param_tok->start;
        uint32_t len = param_tok->len;
        // Very naive parse: find ':' and '}'
        uint32_t i = 0;
        while (i < len && text[i] != ':') i++;
        if (i + 1 >= len) continue; // malformed
        uint32_t name_start = i + 1;
        uint32_t name_len = 0;
        while (name_start + name_len < len && text[name_start + name_len] != '}') name_len++;
        if (name_len == 0) continue;
        // Store to string pool
        if (p->string_pos + name_len + 1 >= 4096) continue;
        uint32_t name_offset = p->string_pos;
        for (uint32_t j = 0; j < name_len; j++) {
            p->string_pool[p->string_pos++] = text[name_start + j];
        }
        p->string_pool[p->string_pos++] = '\0';
        uint16_t id_idx = alloc_node(p, NODE_IDENTIFIER);
        if (id_idx == 0) continue;
        p->nodes[id_idx].data.ident.name_offset = name_offset;
        p->nodes[id_idx].data.ident.name_len = name_len;
        ADD_PARAM_NODE(id_idx);
    }

    // Second form: /{ @param:name }/ pattern parsed token-by-token
    while (match(p, TOK_SLASH)) {
        if (!check(p, TOK_LBRACE)) break;
        advance(p); // consume {
        match(p, TOK_AT); // optional @
        if (!check(p, TOK_IDENTIFIER)) break;
        Token* param_tok = advance(p);
        // optional : name
        Token* name_tok = NULL;
        if (match(p, TOK_COLON) && check(p, TOK_IDENTIFIER)) {
            name_tok = advance(p);
        } else {
            name_tok = param_tok; // use same token
        }
        match(p, TOK_RBRACE); // consume }
        // Store name
        uint32_t name_offset = store_string(p, name_tok);
        uint16_t id_idx = alloc_node(p, NODE_IDENTIFIER);
        if (id_idx == 0) continue;
        p->nodes[id_idx].data.ident.name_offset = name_offset;
        p->nodes[id_idx].data.ident.name_len = name_tok->len;
        ADD_PARAM_NODE(id_idx);
        // trailing slash already consumed at loop start
    }

    // After parameters parsed assign to func_node.right_idx
    p->nodes[func_node].data.binary.right_idx = param_start;

    #undef ADD_PARAM_NODE
    // -----------------------------------------------------------
    
    // Look for < to open function body
    print_str("[PARSER] Looking for < to open function body, current token: ");
    if (p->current < p->count) {
        print_num(p->tokens[p->current].type);
    } else {
        print_str("END");
    }
    print_str("\n");
    
    // Check if we already have an action block (the lexer might have consumed the <)
    if (check(p, TOK_ACTION_START)) {
        print_str("[PARSER] Found action block directly, parsing\n");
        
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
        
    } else if (match(p, TOK_LT)) {
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
        
    } else {
        print_str("[PARSER] Error: Expected < or action block\n");
        p->has_error = true;
        return 0;
    }
    
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
static uint16_t parse_timeline_def(Parser* p) {
    Token* timeline_tok = advance(p); // Consume TOK_TIMELINE_DEF

    uint16_t timeline_node = alloc_node(p, NODE_TIMELINE_DEF);
    if (timeline_node == 0) return 0;

    // Expect pipe-delimited identifier |name|
    if (!match(p, TOK_PIPE)) {
        p->has_error = true;
        return 0;
    }

    Token* name_tok = advance(p);

    if (!match(p, TOK_PIPE)) {
        p->has_error = true;
        return 0;
    }

    uint32_t name_offset = store_string(p, name_tok);
    p->nodes[timeline_node].data.ident.name_offset = name_offset;
    p->nodes[timeline_node].data.ident.name_len = name_tok->len;

    return timeline_node;
}

// Helper function to parse a bracketed section with nested bracket support
static uint16_t parse_bracketed_section(Parser* p, bool is_statement) {
    if (!match(p, TOK_BRACKET_OPEN)) {
        return 0;
    }
    
    // Count nested brackets to find the matching close bracket
    int bracket_depth = 1;
    uint32_t section_start = p->current;
    
    while (!at_end(p) && bracket_depth > 0) {
        if (check(p, TOK_BRACKET_OPEN)) {
            bracket_depth++;
        } else if (check(p, TOK_BRACKET_CLOSE)) {
            bracket_depth--;
        }
        
        if (bracket_depth > 0) {
            advance(p);
        }
    }
    
    if (bracket_depth > 0) {
        print_str("[PARSER] ERROR: Unclosed bracket in for loop section\n");
        p->has_error = true;
        return 0;
    }
    
    // Now parse the section between the brackets
    uint32_t section_end = p->current;
    uint32_t saved_current = p->current;
    p->current = section_start;
    
    uint16_t result;
    if (is_statement) {
        result = parse_statement(p);
    } else {
        result = parse_expression(p);
    }
    
    // Skip to after the closing bracket
    p->current = section_end;
    advance(p); // consume the closing bracket
    
    return result;
}

// Parse while loop: f.whl-[condition]/
static uint16_t parse_while_loop(Parser* p) {
    advance(p); // consume while
    
    uint16_t while_node = alloc_node(p, NODE_WHILE_LOOP);
    if (while_node == 0) return 0;
    
    // Expect '-' after while
    if (!match(p, TOK_MINUS)) {
        print_str("[PARSER] ERROR: Expected '-' after while\n");
        p->has_error = true;
        return 0;
    }
    
    // Expect '[' to start condition
    if (!match(p, TOK_BRACKET_OPEN)) {
        print_str("[PARSER] ERROR: Expected '[' after while-\n");
        p->has_error = true;
        return 0;
    }
    
    // Parse condition expression
    uint16_t condition = parse_expression(p);
    
    // Expect ']' to close condition
    if (!match(p, TOK_BRACKET_CLOSE)) {
        print_str("[PARSER] ERROR: Expected ']' after while condition\n");
        p->has_error = true;
        return 0;
    }
    
    p->nodes[while_node].data.while_loop.condition_idx = condition;
    
    // Expect '/' to start body
    if (!match(p, TOK_DIV)) {
        print_str("[PARSER] ERROR: Expected '/' after while condition\n");
        p->has_error = true;
        return 0;
    }
    
    // Parse body statements until standalone '\\'
    uint16_t body_start = 0;
    uint16_t body_end = 0;
    
    print_str("[PARSER] Starting while loop body parsing\n");
    while (!at_end(p)) {
        print_str("[PARSER] Body loop: checking token type=");
        print_num(peek(p)->type);
        print_str(" pos=");
        print_num(p->current);
        print_str("\n");
        
        // Check if we've reached a standalone backslash (loop terminator)
        if (check(p, TOK_BACKSLASH)) {
            print_str("[PARSER] Found backslash, ending body parsing\n");
            break;
        }
        
        uint16_t stmt = parse_statement(p);
        print_str("[PARSER] parse_statement returned: ");
        print_num(stmt);
        print_str("\n");
        if (stmt == 0) {
            print_str("[PARSER] parse_statement failed, breaking from body loop\n");
            break;
        }
        
        if (body_start == 0) {
            body_start = stmt;
            body_end = stmt;
            print_str("[PARSER] Set body_start=");
            print_num(body_start);
            print_str("\n");
        } else {
            // Chain statements together
            p->nodes[body_end].data.binary.right_idx = stmt;
            body_end = stmt;
            print_str("[PARSER] Chained statement, body_end=");
            print_num(body_end);
            print_str("\n");
        }
    }
    print_str("[PARSER] Finished body parsing loop, body_start=");
    print_num(body_start);
    print_str("\n");
    
    // Expect '\\' to end body
    print_str("[PARSER] About to check for closing backslash, current token=");
    print_num(peek(p)->type);
    print_str("\n");
    if (!match(p, TOK_BACKSLASH)) {
        print_str("[PARSER] ERROR: Expected '\\' after while body\n");
        p->has_error = true;
        return 0;
    }
    
    print_str("[PARSER] While loop parsing complete: body_start=");
    print_num(body_start);
    print_str(" current_token=");
    print_num(p->current);
    print_str("\n");
    
    p->nodes[while_node].data.while_loop.body_idx = body_start;
    return while_node;
}

// Parse for loop: for-[init]-[condition]-[increment]/
static uint16_t parse_for_loop(Parser* p) {
    advance(p); // consume for
    
    uint16_t for_node = alloc_node(p, NODE_FOR_LOOP);
    if (for_node == 0) return 0;
    
    // Expect '-' after for
    if (!match(p, TOK_MINUS)) {
        print_str("[PARSER] ERROR: Expected '-' after for\n");
        p->has_error = true;
        return 0;
    }
    
    // Parse initialization: [init] - using helper for nested brackets
    uint16_t init = parse_bracketed_section(p, true); // true = statement
    if (init == 0) {
        print_str("[PARSER] ERROR: Failed to parse for init section\n");
        p->has_error = true;
        return 0;
    }
    
    // Expect '-' separator
    if (!match(p, TOK_MINUS)) {
        print_str("[PARSER] ERROR: Expected '-' after for init\n");
        p->has_error = true;
        return 0;
    }
    
    // Parse condition: [condition] - using helper for nested brackets
    uint16_t condition = parse_bracketed_section(p, false); // false = expression
    if (condition == 0) {
        print_str("[PARSER] ERROR: Failed to parse for condition section\n");
        p->has_error = true;
        return 0;
    }
    
    // Expect '-' separator
    if (!match(p, TOK_MINUS)) {
        print_str("[PARSER] ERROR: Expected '-' after for condition\n");
        p->has_error = true;
        return 0;
    }
    
    // Parse increment: [increment] - using helper for nested brackets
    uint16_t increment = parse_bracketed_section(p, true); // true = statement (for var assignment)
    if (increment == 0) {
        print_str("[PARSER] ERROR: Failed to parse for increment section\n");
        p->has_error = true;
        return 0;
    }
    
    p->nodes[for_node].data.for_loop.init_idx = init;
    p->nodes[for_node].data.for_loop.condition_idx = condition;
    p->nodes[for_node].data.for_loop.increment_idx = increment;
    
    // Expect '/' to start body
    if (!match(p, TOK_DIV)) {
        print_str("[PARSER] ERROR: Expected '/' after for parameters\n");
        p->has_error = true;
        return 0;
    }
    
    // Parse body statements until standalone '\\'
    uint16_t body_start = 0;
    uint16_t body_end = 0;
    
    while (!at_end(p)) {
        // Check if we've reached a standalone backslash (loop terminator)
        if (check(p, TOK_BACKSLASH)) {
            // Peek ahead to see if this is a standalone backslash
            // (not part of a statement that was already parsed)
            break;
        }
        
        uint16_t stmt = parse_statement(p);
        if (stmt == 0) break;
        
        if (body_start == 0) {
            body_start = stmt;
            body_end = stmt;
        } else {
            // Chain statements together
            p->nodes[body_end].data.binary.right_idx = stmt;
            body_end = stmt;
        }
    }
    
    // Expect '\\' to end body
    if (!match(p, TOK_BACKSLASH)) {
        print_str("[PARSER] ERROR: Expected '\\' after for body\n");
        p->has_error = true;
        return 0;
    }
    
    p->nodes[for_node].data.for_loop.body_idx = body_start;
    return for_node;
}

static uint16_t parse_conditional(Parser* p) {
    // Advance the conditional token (if, while, etc.)
    Token* cond_tok = advance(p);
    
    uint16_t cond_node = alloc_node(p, NODE_CONDITIONAL);
    if (cond_node == 0) return 0;
    
    // Store the conditional type
    p->nodes[cond_node].data.binary.op = cond_tok->type;
    
    print_str("[PARSER] parse_conditional: type=");
    print_num(cond_tok->type);
    print_str("\n");
    
    // Expect '/' after conditional keyword
    if (!match(p, TOK_DIV)) {
        print_str("[PARSER] ERROR: Expected '/' after conditional keyword\n");
        p->has_error = true;
        return 0;
    }
    
    // Parse the condition expression
    uint16_t condition = parse_expression(p);
    p->nodes[cond_node].data.binary.left_idx = condition;
    
    print_str("[PARSER] parse_conditional: condition parsed, node=");
    print_num(condition);
    print_str("\n");
    
    // Expect '<' to start the body
    print_str("[PARSER] About to match TOK_LT, current position=");
    print_num(p->current);
    print_str(" total_tokens=");
    print_num(p->count);
    if (p->current < p->count) {
        print_str(" current_token_type=");
        print_num(p->tokens[p->current].type);
        print_str(" current_token_text='");
        Token* cur_tok = &p->tokens[p->current];
        for (uint32_t i = 0; i < cur_tok->len && i < 10; i++) {
            char c = p->source[cur_tok->start + i];
            if (c >= 32 && c <= 126) {
                char buf[2] = {c, '\0'};
                print_str(buf);
            } else {
                print_str("?");
            }
        }
        print_str("'");
    }
    print_str("\n");
    
    if (!match(p, TOK_LT)) {
        print_str("[PARSER] ERROR: Expected '<' after condition\n");
        p->has_error = true;
        return 0;
    }
    
    // Parse the body statements until we hit ':>'
    uint16_t body_start = 0;
    uint16_t body_end = 0;
    
    while (!at_end(p) && !check(p, TOK_BLOCK_END)) {
        uint16_t stmt = parse_statement(p);
        if (stmt == 0) break;
        
        if (body_start == 0) {
            body_start = stmt;
        } else if (body_end != 0) {
            // Chain statements together
            p->nodes[body_end].data.binary.right_idx = stmt;
        }
        body_end = stmt;
    }
    
    // Store the body in the conditional node
    p->nodes[cond_node].data.binary.right_idx = body_start;
    
    // Expect ':>' to end the block
    if (!match(p, TOK_BLOCK_END)) {
        print_str("[PARSER] ERROR: Expected ':>' to end conditional block\n");
        p->has_error = true;
        return 0;
    }
    
    print_str("[PARSER] parse_conditional: body parsed successfully\n");
    
    // Check for 'else' clause (only for if statements)
    if (cond_tok->type == TOK_COND_IF && check(p, TOK_ELSE)) {
        print_str("[PARSER] parse_conditional: found else clause\n");
        advance(p); // consume 'else'
        
        // Expect '<' to start else body
        if (!match(p, TOK_LT)) {
            print_str("[PARSER] ERROR: Expected '<' after else\n");
            p->has_error = true;
            return 0;
        }
        
        // Create an else node and chain it
        uint16_t else_node = alloc_node(p, NODE_CONDITIONAL);
        if (else_node == 0) return 0;
        
        p->nodes[else_node].data.binary.op = TOK_ELSE;
        p->nodes[else_node].data.binary.left_idx = 0; // No condition for else
        
        // Parse else body
        uint16_t else_body_start = 0;
        uint16_t else_body_end = 0;
        
        while (!at_end(p) && !check(p, TOK_BLOCK_END)) {
            uint16_t stmt = parse_statement(p);
            if (stmt == 0) break;
            
            if (else_body_start == 0) {
                else_body_start = stmt;
            } else if (else_body_end != 0) {
                p->nodes[else_body_end].data.binary.right_idx = stmt;
            }
            else_body_end = stmt;
        }
        
        p->nodes[else_node].data.binary.right_idx = else_body_start;
        
        // Expect ':>' to end the else block
        if (!match(p, TOK_BLOCK_END)) {
            print_str("[PARSER] ERROR: Expected ':>' to end else block\n");
            p->has_error = true;
            return 0;
        }
        
        print_str("[PARSER] parse_conditional: else body parsed successfully\n");
        
        // Chain the else node to the if node using a special field
        // We'll use the upper bits of left_idx to store the else clause
        p->nodes[cond_node].data.timing.temporal_offset = else_node;
    }
    
    return cond_node;
}

// Parse statement
static uint16_t parse_statement(Parser* p) {
    if (at_end(p)) return 0;
    
    // Skip comments
    Token* tok = peek(p);
    while (tok && tok->type == TOK_COMMENT) {
        print_str("[PARSER] Skipping comment token at pos ");
        print_num(p->current);
        print_str("\n");
        advance(p);
        tok = peek(p);
    }
    // Skip error tokens
    while (tok && tok->type == TOK_ERROR) {
        print_str("[PARSER] Skipping error token at pos ");
        print_num(p->current);
        print_str("\n");
        advance(p);
        tok = peek(p);
    }
    
    // Skip standalone division tokens (they should be part of output methods)
    while (tok && tok->type == TOK_DIV) {
        print_str("[PARSER] Skipping standalone division token at pos ");
        print_num(p->current);
        print_str("\n");
        advance(p);
        tok = peek(p);
    }
    
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
    
    // Detect generic GGGX command: identifier "gggx" followed by '.'
    if (check(p, TOK_IDENTIFIER)) {
        Token* id_tok = peek(p);
        if (id_tok && id_tok->len == 4 && strncmp(p->source + id_tok->start, "gggx", 4) == 0) {
            Token* dot_tok = peek2(p);
            if (dot_tok && dot_tok->type == TOK_DOT) {
                print_str("[PARSER] Detected generic GGGX command\n");
                return parse_gggx_generic_command(p);
            }
        }
    }
    
    // Variable definition (all types)
    if (check(p, TOK_VAR) || check(p, TOK_VAR_INT) || 
        check(p, TOK_VAR_FLOAT) || check(p, TOK_VAR_STRING) || 
        check(p, TOK_VAR_BOOL) || check(p, TOK_VAR_SOLID)) {
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
    
    // While loop
    if (check(p, TOK_COND_WHL)) {
        return parse_while_loop(p);
    }
    
    // For loop
    if (check(p, TOK_COND_FOR)) {
        return parse_for_loop(p);
    }
    
    // Conditional - check for all other conditional tokens
    if (check(p, TOK_FUNC_CAN) || check(p, TOK_COND_IF) || 
        check(p, TOK_COND_ENS) || check(p, TOK_COND_VER) ||
        check(p, TOK_COND_CHK) || check(p, TOK_COND_TRY) ||
        check(p, TOK_COND_GRD) || check(p, TOK_COND_UNL) ||
        check(p, TOK_COND_UNT)) {
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
    
    // GGGX commands
    if (check(p, TOK_GGGX_INIT) || check(p, TOK_GGGX_GO) || check(p, TOK_GGGX_GET) || 
        check(p, TOK_GGGX_GAP) || check(p, TOK_GGGX_GLIMPSE) || check(p, TOK_GGGX_GUESS) ||
        check(p, TOK_GGGX_ANALYZE) || check(p, TOK_GGGX_SET) || check(p, TOK_GGGX_ENABLE) ||
        check(p, TOK_GGGX_STATUS) || check(p, TOK_GGGX_PRINT)) {
        return parse_gggx_command(p);
    }
    
    // Function call: identifier followed by /
    if (check(p, TOK_IDENTIFIER) && peek2(p) && peek2(p)->type == TOK_SLASH) {
        Token* name_tok = advance(p); // consume identifier
        advance(p); // consume /
        
        uint16_t call_node = alloc_node(p, NODE_FUNC_CALL);
        if (call_node == 0) return 0;
        
        // Create identifier node for the function name
        uint16_t name_node = alloc_node(p, NODE_IDENTIFIER);
        if (name_node == 0) return 0;
        
        // Store name in string pool
        uint32_t name_offset = p->string_pos;
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
        
        // Store function name in left_idx
        p->nodes[call_node].data.binary.left_idx = name_node;
        // No arguments for now
        p->nodes[call_node].data.binary.right_idx = 0;
        
        return call_node;
    }
    // Output methods
    if (check(p, TOK_PRINT) || check(p, TOK_TXT) || check(p, TOK_OUT) || 
        check(p, TOK_FMT) || check(p, TOK_DYN)) {
        TokenType output_type = advance(p)->type;
        print_str("[PARSER-DEBUG] Entered output method parse_statement for output_type=");
        print_num(output_type);
        print_str("\n");
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
        
        // Parse content - handle different content types
        if (check(p, TOK_STRING)) {
            // Parse string normally if it's a single token
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
        } else if (check(p, TOK_IDENTIFIER) || check(p, TOK_VAR) || 
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
                }
            }
            
            // Store name in string pool
            uint32_t name_offset = p->string_pos;
            for (uint32_t i = 0; i < name_len; i++) {
                p->string_pool[p->string_pos++] = p->source[name_start + i];
            }
            p->string_pool[p->string_pos++] = '\0';
            
            p->nodes[id_node].data.ident.name_offset = name_offset;
            p->nodes[id_node].data.ident.name_len = name_len;
            
            p->nodes[output_node].data.output.content_idx = id_node;
        } else if (peek(p) && peek(p)->type == 138) { // Quote token
            // Handle string that was tokenized as separate parts: " + words + "
            advance(p); // consume opening quote
            
            // Collect all tokens until closing quote
            uint32_t str_start = p->string_pos;
            while (!at_end(p) && peek(p) && peek(p)->type != 138 && peek(p)->type != TOK_BACKSLASH) {
                Token* word_tok = advance(p);
                
                // Add space between words (except for punctuation)
                if (p->string_pos > str_start && word_tok->type != 58) { // 58 = '!'
                    p->string_pool[p->string_pos++] = ' ';
                }
                
                // Copy word content
                for (uint32_t i = 0; i < word_tok->len; i++) {
                    p->string_pool[p->string_pos++] = p->source[word_tok->start + i];
                }
            }
            
            // Consume closing quote if present
            if (peek(p) && peek(p)->type == 138) {
                advance(p);
            }
            
            // Null terminate the reconstructed string
            p->string_pool[p->string_pos++] = '\0';
            uint32_t str_len = p->string_pos - str_start - 1; // Exclude null terminator
            
            // Create string node
            uint16_t str_node = alloc_node(p, NODE_STRING);
            if (str_node == 0) return 0;
            
            p->nodes[str_node].data.ident.name_offset = str_start;
            p->nodes[str_node].data.ident.name_len = str_len;
            
            print_str("[PARSER] Created reconstructed NODE_STRING at idx=");
            print_num(str_node);
            print_str(" len=");
            print_num(str_len);
            print_str("\n");
            
            p->nodes[output_node].data.output.content_idx = str_node;
        } else if (check(p, TOK_NUMBER) || check(p, TOK_MINUS) || 
                   check(p, TOK_LPAREN) || check(p, TOK_MATH_PREFIX) ||
                   check(p, TOK_SOLID_NUMBER)) {
            // Parse expression (could be number, arithmetic, math function, solid number, etc.)
            uint16_t expr_node = parse_expression(p);
            p->nodes[output_node].data.output.content_idx = expr_node;
        } else {
            // No content - set to invalid
            p->nodes[output_node].data.output.content_idx = 0xFFFF;
        }
        
        // Require ending backslash
        if (!check(p, TOK_BACKSLASH)) {
            p->has_error = true;
            print_str("[PARSER] ERROR: Print statement requires closing backslash\n");
            return 0;
        }
        advance(p);  // Consume the backslash
        
        // Explicitly set next_output to 0 for the last statement
        p->nodes[output_node].data.output.next_output = 0;
        
        return output_node;
    }
    
    // Return statement (return/ expr \)
    if (check(p, TOK_RETURN)) {
        advance(p); // consume return/
        // Consume optional slash token if lexer split it
        if (check(p, TOK_SLASH) || check(p, TOK_DIV)) {
            advance(p);
        }
        
        uint16_t expr_node = 0;
        // If the next token is not a backslash, parse the return expression
        if (!check(p, TOK_BACKSLASH)) {
            expr_node = parse_expression(p);
        }
        
        // Require terminating backslash
        if (!check(p, TOK_BACKSLASH)) {
            p->has_error = true;
            print_str("[PARSER] ERROR: Return statement requires closing backslash\n");
            return 0;
        }
        advance(p); // consume backslash
        
        // Create NODE_RETURN and store expression index
        uint16_t ret_node = alloc_node(p, NODE_RETURN);
        if (ret_node == 0) return 0;
        p->nodes[ret_node].data.binary.left_idx = expr_node; // expression (can be 0)
        return ret_node;
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
    
    // Check for identifier : identifier pattern (documentation/comment lines)
    if (check(p, TOK_IDENTIFIER)) {
        Token* first_id = peek(p);
        Token* colon_tok = peek2(p);
        Token* second_id = peek3(p);
        
        if (colon_tok && colon_tok->type == TOK_COLON && second_id && second_id->type == TOK_IDENTIFIER) {
            print_str("[PARSER-STMT] Found identifier:identifier pattern, skipping as documentation\n");
            advance(p); // consume first identifier
            advance(p); // consume colon
            advance(p); // consume second identifier
            return 0xFFFF; // Skip this line
        }
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

// === SMART CONTENT FILTERING HELPERS ===
#include <string.h>

// Helper: Check if token is start of documentation/non-Blaze line
static int is_documentation_line(Token* token, const char* source) {
    if (!token) return 0;
    // Skip lines starting with #
    if (token->type == TOK_COMMENT) return 1;
    // Skip lines starting with identifier 'include' or 'using'
    if (token->type == TOK_IDENTIFIER) {
        const char* val = source + token->start;
        if ((token->len == 7 && strncmp(val, "include", 7) == 0) ||
            (token->len == 5 && strncmp(val, "using", 5) == 0)) {
            return 1;
        }
    }
    return 0;
}

// Helper: Check if token is a valid Blaze statement start
static int is_blaze_statement_start(Token* token, const char* source) {
    if (!token) return 0;
    if (token->type == TOK_IDENTIFIER) {
        const char* val = source + token->start;
        if ((token->len == 3 && strncmp(val, "var", 3) == 0) ||
            (token->len == 4 && strncmp(val, "fucn", 4) == 0) ||
            (token->len == 2 && strncmp(val, "do", 2) == 0) ||
            (token->len == 8 && strncmp(val, "timeline", 8) == 0) ||
            (token->len == 3 && strncmp(val, "gap", 3) == 0) ||
            (token->len == 4 && strncmp(val, "gggx", 4) == 0)) {
            return 1;
        }
    }
    // Special Blaze tokens and output methods
    if (token->type == TOK_PIPE || token->type == TOK_JUMP_MARKER || token->type == TOK_BANG || token->type == TOK_COMMENT ||
        token->type == TOK_PRINT || token->type == TOK_TXT || token->type == TOK_OUT || token->type == TOK_FMT || token->type == TOK_DYN ||
        token->type == TOK_GGGX_INIT || token->type == TOK_GGGX_GO || token->type == TOK_GGGX_GET || token->type == TOK_GGGX_GAP ||
        token->type == TOK_GGGX_GLIMPSE || token->type == TOK_GGGX_GUESS || token->type == TOK_GGGX_ANALYZE ||
        token->type == TOK_GGGX_SET || token->type == TOK_GGGX_ENABLE || token->type == TOK_GGGX_STATUS || token->type == TOK_GGGX_PRINT) {
        return 1;
    }
    return 0;
}

// Helper: Check if token is standalone punctuation noise
static int should_skip_standalone_token(Token* token) {
    if (!token) return 0;
    switch (token->type) {
        case TOK_COMMA:
        case TOK_SEMICOLON:
        case TOK_DOT:
        case TOK_GT:
        case TOK_LT:
        case TOK_COLON:
        case TOK_EQUALS:
            return 1;
        default:
            return 0;
    }
}

// Helper: Skip to end of line (advance until newline or EOF)
static void skip_to_end_of_line(Parser* parser, const char* source) {
    if (at_end(parser)) return;
    
    // Get the current line start position by looking at the current token
    Token* start_token = &parser->tokens[parser->current];
    uint32_t current_line_start = start_token->start;
    
    // Find the start of the current line by scanning backwards in source
    while (current_line_start > 0 && source[current_line_start - 1] != '\n') {
        current_line_start--;
    }
    
    // Now advance tokens until we find one that starts on a different line
    while (!at_end(parser)) {
        Token* t = &parser->tokens[parser->current];
        if (t->type == TOK_EOF) {
            break;
        }
        
        // Find the line start for this token
        uint32_t token_line_start = t->start;
        while (token_line_start > 0 && source[token_line_start - 1] != '\n') {
            token_line_start--;
        }
        
        // If this token is on a different line, stop
        if (token_line_start != current_line_start) {
            break;
        }
        
        // Advance to next token
        parser->current++;
        
        // Safety check to prevent infinite loops
        if (parser->current >= parser->count) {
            break;
        }
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
    
    // Initialize parser
    parser_init(tokens, count, node_pool, pool_size, string_pool, source);
    
    // Create root program node
    uint16_t program_node = alloc_node(&parser, NODE_PROGRAM);
    if (program_node == 0) {
        print_str("[PARSER] Failed to allocate program node\n");
        return 0;
    }
    
    // === MAIN PARSER LOOP: SMART CONTENT FILTERING ===
    uint16_t first_stmt = 0;
    uint16_t last_stmt = 0;
    while (!at_end(&parser)) {
        print_str("[PARSER] Loop iteration: current=");
        print_num(parser.current);
        print_str(" count=");
        print_num(parser.count);
        print_str("\n");
        
        // TIER 1: Skip documentation/non-Blaze lines
        Token* current_tok = peek(&parser);
        if (current_tok && is_documentation_line(current_tok, parser.source)) {
            print_str("[PARSER] Skipping documentation line\n");
            skip_to_end_of_line(&parser, parser.source);
            continue;
        }
        
        // DEBUG: Print current token after skipping documentation
        if (current_tok) {
            print_str("[PARSER] Current token: type=");
            print_num(current_tok->type);
            print_str(" start=");
            print_num(current_tok->start);
            print_str(" len=");
            print_num(current_tok->len);
            print_str(" text='");
            for (uint16_t i = 0; i < current_tok->len && i < 20; i++) {
                char c = parser.source[current_tok->start + i];
                char buf[2] = {c, '\0'};
                print_str(buf);
            }
            print_str("'\n");
        } else {
            print_str("[PARSER] Current token: NULL\n");
        }
        
        // TIER 2: Parse valid Blaze statements
        if (current_tok && is_blaze_statement_start(current_tok, parser.source)) {
            print_str("[PARSER] Parsing Blaze statement\n");
            uint16_t stmt = parse_statement(&parser);
            if (stmt != 0xFFFF) {
                // Chain statements to program node
                print_str("[PARSER] Chaining statement ");
                print_num(stmt);
                print_str(" (first_stmt=");
                print_num(first_stmt);
                print_str(", last_stmt=");
                print_num(last_stmt);
                print_str(")\n");
                
                if (first_stmt == 0) {
                    parser.nodes[program_node].data.binary.left_idx = stmt;
                    first_stmt = stmt;
                    print_str("[PARSER] Set as first statement\n");
                            } else if (last_stmt != 0 && last_stmt < parser.node_count) {
                // Don't link to conditional nodes - they use right_idx for body
                if (parser.nodes[last_stmt].type != NODE_CONDITIONAL && parser.nodes[last_stmt].type != NODE_FUNC_DEF) {
                    parser.nodes[last_stmt].data.binary.right_idx = stmt;
                    print_str("[PARSER] Linked to previous statement ");
                    print_num(last_stmt);
                    print_str("\n");
                } else {
                    print_str("[PARSER] Skipping link to conditional node ");
                    print_num(last_stmt);
                    print_str(" (preserving body reference)\n");
                }
                
                // Update program node's right_idx to point to the last statement in the chain
                parser.nodes[program_node].data.binary.right_idx = stmt;
                print_str("[PARSER] Updated program right_idx to last statement ");
                print_num(stmt);
                print_str("\n");
                } else {
                    print_str("[PARSER] WARNING: Could not link statement (last_stmt=");
                    print_num(last_stmt);
                    print_str(", node_count=");
                    print_num(parser.node_count);
                    print_str(")\n");
                }
                last_stmt = stmt;
                print_str("[PARSER] Successfully parsed statement: ");
                print_num(stmt);
                print_str("\n");
            }
            continue;
        }
        
        // TIER 3: Skip standalone punctuation noise
        if (current_tok && should_skip_standalone_token(current_tok)) {
            print_str("[PARSER] Skipping standalone punctuation: ");
            print_num(current_tok->type);
            print_str("\n");
            advance(&parser);
            continue;
        }
        
        // If we get here, try to parse as a general statement
        print_str("[PARSER] Trying general statement parsing\n");
        uint16_t stmt = parse_statement(&parser);
        if (stmt != 0xFFFF) {
            // Chain statements to program node
            print_str("[PARSER] Chaining general statement ");
            print_num(stmt);
            print_str(" (first_stmt=");
            print_num(first_stmt);
            print_str(", last_stmt=");
            print_num(last_stmt);
            print_str(")\n");
            
            if (first_stmt == 0) {
                parser.nodes[program_node].data.binary.left_idx = stmt;
                first_stmt = stmt;
                print_str("[PARSER] Set as first statement\n");
            } else if (last_stmt != 0 && last_stmt < parser.node_count) {
                // Don't link to conditional nodes - they use right_idx for body
                if (parser.nodes[last_stmt].type != NODE_CONDITIONAL && parser.nodes[last_stmt].type != NODE_FUNC_DEF) {
                    parser.nodes[last_stmt].data.binary.right_idx = stmt;
                    print_str("[PARSER] Linked to previous statement ");
                    print_num(last_stmt);
                    print_str("\n");
                } else {
                    print_str("[PARSER] Skipping link to conditional node ");
                    print_num(last_stmt);
                    print_str(" (preserving body reference)\n");
                }
                
                // Update program node's right_idx to point to the last statement in the chain
                parser.nodes[program_node].data.binary.right_idx = stmt;
                print_str("[PARSER] Updated program right_idx to last statement ");
                print_num(stmt);
                print_str("\n");
            } else {
                print_str("[PARSER] WARNING: Could not link statement (last_stmt=");
                print_num(last_stmt);
                print_str(", node_count=");
                print_num(parser.node_count);
                print_str(")\n");
            }
            last_stmt = stmt;
            print_str("[PARSER] Successfully parsed general statement: ");
            print_num(stmt);
            print_str("\n");
        } else {
            print_str("[PARSER] General statement parsing failed, advancing token\n");
            advance(&parser);
        }
    }
    
    print_str("[PARSER] Parsing complete. Program node: ");
    print_num(program_node);
    print_str("\n");
    
    return program_node;
}

// Parse GGGX commands
static uint16_t parse_gggx_command(Parser* p) {
    print_str("[PARSER] Parsing GGGX command\n");
    
    // Check for GGGX tokens
    if (check(p, TOK_GGGX_INIT)) {
        advance(p); // consume gggx.init
        // Optional slash after command
        match(p, TOK_SLASH);
        
        // Create GGGX init node
        uint16_t gggx_node = alloc_node(p, NODE_FUNC_CALL);
        if (gggx_node == 0) return 0;
        
        // Create identifier node for "gggx_init"
        uint16_t name_node = alloc_node(p, NODE_IDENTIFIER);
        if (name_node == 0) return 0;
        
        // Store "gggx_init" in string pool
        uint32_t name_offset = p->string_pos;
        const char* name = "gggx_init";
        uint32_t name_len = 9;
        
        for (uint32_t i = 0; i < name_len; i++) {
            p->string_pool[p->string_pos++] = name[i];
        }
        p->string_pool[p->string_pos++] = '\0';
        
        p->nodes[name_node].data.ident.name_offset = name_offset;
        p->nodes[name_node].data.ident.name_len = name_len;
        
        p->nodes[gggx_node].data.binary.left_idx = name_node;
        p->nodes[gggx_node].data.binary.right_idx = 0; // No arguments
        
        // Consume trailing backslash if present
        if (check(p, TOK_BACKSLASH)) {
            advance(p);
        }
        return gggx_node;
    }
    
    if (check(p, TOK_GGGX_GO) || check(p, TOK_GGGX_GET) || check(p, TOK_GGGX_GAP) || 
        check(p, TOK_GGGX_GLIMPSE) || check(p, TOK_GGGX_GUESS)) {
        
        TokenType gggx_type = peek(p)->type;
        advance(p); // consume gggx.phase
        
        if (!match(p, TOK_SLASH)) {
            p->has_error = true;
            return 0;
        }
        
        // Parse arguments (value, precision)
        uint16_t value_arg = parse_expression(p);
        if (value_arg == 0) {
            p->has_error = true;
            return 0;
        }
        
        if (!match(p, TOK_COMMA)) {
            p->has_error = true;
            return 0;
        }
        
        uint16_t precision_arg = parse_expression(p);
        if (precision_arg == 0) {
            p->has_error = true;
            return 0;
        }
        
        if (!match(p, TOK_SLASH)) {
            p->has_error = true;
            return 0;
        }
        
        // Create GGGX phase call node
        uint16_t gggx_node = alloc_node(p, NODE_FUNC_CALL);
        if (gggx_node == 0) return 0;
        
        // Create identifier node for the phase name
        uint16_t name_node = alloc_node(p, NODE_IDENTIFIER);
        if (name_node == 0) return 0;
        
        // Store phase name in string pool
        uint32_t name_offset = p->string_pos;
        const char* phase_names[] = {"gggx_go", "gggx_get", "gggx_gap", "gggx_glimpse", "gggx_guess"};
        const char* phase_name = phase_names[gggx_type - TOK_GGGX_GO];
        uint32_t name_len = strlen(phase_name);
        
        for (uint32_t i = 0; i < name_len; i++) {
            p->string_pool[p->string_pos++] = phase_name[i];
        }
        p->string_pool[p->string_pos++] = '\0';
        
        p->nodes[name_node].data.ident.name_offset = name_offset;
        p->nodes[name_node].data.ident.name_len = name_len;
        
        // Create argument list node
        uint16_t args_node = alloc_node(p, NODE_BINARY_OP);
        if (args_node == 0) return 0;
        
        p->nodes[args_node].data.binary.op = TOK_COMMA;
        p->nodes[args_node].data.binary.left_idx = value_arg;
        p->nodes[args_node].data.binary.right_idx = precision_arg;
        
        p->nodes[gggx_node].data.binary.left_idx = name_node;
        p->nodes[gggx_node].data.binary.right_idx = args_node;
        
        return gggx_node;
    }
    
    if (check(p, TOK_GGGX_ANALYZE)) {
        advance(p); // consume gggx.analyze
        
        if (!match(p, TOK_SLASH)) {
            p->has_error = true;
            return 0;
        }
        
        // Parse arguments (value, precision)
        uint16_t value_arg = parse_expression(p);
        if (value_arg == 0) {
            p->has_error = true;
            return 0;
        }
        
        if (!match(p, TOK_COMMA)) {
            p->has_error = true;
            return 0;
        }
        
        uint16_t precision_arg = parse_expression(p);
        if (precision_arg == 0) {
            p->has_error = true;
            return 0;
        }
        
        if (!match(p, TOK_SLASH)) {
            p->has_error = true;
            return 0;
        }
        
        // Create GGGX analyze node
        uint16_t gggx_node = alloc_node(p, NODE_FUNC_CALL);
        if (gggx_node == 0) return 0;
        
        // Create identifier node for "gggx_analyze_with_control"
        uint16_t name_node = alloc_node(p, NODE_IDENTIFIER);
        if (name_node == 0) return 0;
        
        // Store name in string pool
        uint32_t name_offset = p->string_pos;
        const char* name = "gggx_analyze_with_control";
        uint32_t name_len = 24;
        
        for (uint32_t i = 0; i < name_len; i++) {
            p->string_pool[p->string_pos++] = name[i];
        }
        p->string_pool[p->string_pos++] = '\0';
        
        p->nodes[name_node].data.ident.name_offset = name_offset;
        p->nodes[name_node].data.ident.name_len = name_len;
        
        // Create argument list node
        uint16_t args_node = alloc_node(p, NODE_BINARY_OP);
        if (args_node == 0) return 0;
        
        p->nodes[args_node].data.binary.op = TOK_COMMA;
        p->nodes[args_node].data.binary.left_idx = value_arg;
        p->nodes[args_node].data.binary.right_idx = precision_arg;
        
        p->nodes[gggx_node].data.binary.left_idx = name_node;
        p->nodes[gggx_node].data.binary.right_idx = args_node;
        
        return gggx_node;
    }
    
    if (check(p, TOK_GGGX_SET) || check(p, TOK_GGGX_ENABLE) || check(p, TOK_GGGX_STATUS) || check(p, TOK_GGGX_PRINT)) {
        TokenType gggx_type = peek(p)->type;
        advance(p); // consume gggx.command
        
        if (!match(p, TOK_SLASH)) {
            p->has_error = true;
            return 0;
        }
        
        // Parse arguments based on command type
        uint16_t arg1 = 0, arg2 = 0;
        
        if (gggx_type == TOK_GGGX_SET) {
            // gggx.set_go_phase/ function_name \
            arg1 = parse_expression(p);
            if (arg1 == 0) {
                p->has_error = true;
                return 0;
            }
        } else if (gggx_type == TOK_GGGX_ENABLE) {
            // gggx.enable.go/ true |
            arg1 = parse_expression(p);
            if (arg1 == 0) {
                p->has_error = true;
                return 0;
            }
        } else if (gggx_type == TOK_GGGX_STATUS) {
            // gggx.status.go/ or gggx.status.sub_step/ "name" /
            arg1 = parse_expression(p);
            if (arg1 == 0) {
                p->has_error = true;
                return 0;
            }
        }
        // TOK_GGGX_PRINT has no arguments
        
        if (gggx_type != TOK_GGGX_PRINT) {
            if (!match(p, TOK_SLASH)) {
                p->has_error = true;
                return 0;
            }
        }
        
        // Create GGGX command node
        uint16_t gggx_node = alloc_node(p, NODE_FUNC_CALL);
        if (gggx_node == 0) return 0;
        
        // Create identifier node for the command name
        uint16_t name_node = alloc_node(p, NODE_IDENTIFIER);
        if (name_node == 0) return 0;
        
        // Store command name in string pool
        uint32_t name_offset = p->string_pos;
        const char* command_names[] = {"gggx_set", "gggx_enable", "gggx_status", "gggx_print"};
        const char* command_name = command_names[gggx_type - TOK_GGGX_SET];
        uint32_t name_len = strlen(command_name);
        
        for (uint32_t i = 0; i < name_len; i++) {
            p->string_pool[p->string_pos++] = command_name[i];
        }
        p->string_pool[p->string_pos++] = '\0';
        
        p->nodes[name_node].data.ident.name_offset = name_offset;
        p->nodes[name_node].data.ident.name_len = name_len;
        
        p->nodes[gggx_node].data.binary.left_idx = name_node;
        p->nodes[gggx_node].data.binary.right_idx = arg1;
        
        return gggx_node;
    }
    
    return 0;
}

// === NEW GENERIC GGGX PARSER ===
static uint16_t parse_gggx_generic_command(Parser* p) {
    // Consume 'gggx'
    Token* gtok = advance(p);
    (void)gtok; // unused
    // Expect '.'
    if (!match(p, TOK_DOT)) {
        p->has_error = true;
        return 0;
    }
    // Expect command identifier
    if (!check(p, TOK_IDENTIFIER)) {
        p->has_error = true;
        return 0;
    }
    Token* cmd_tok = advance(p);
    // Build function name string: gggx_<command>
    char temp_name[64];
    uint32_t cmd_len = cmd_tok->len;
    if (cmd_len + 5 >= sizeof(temp_name)) cmd_len = sizeof(temp_name) - 6;
    temp_name[0] = 'g'; temp_name[1] = 'g'; temp_name[2] = 'g'; temp_name[3] = 'x'; temp_name[4] = '_';
    for (uint32_t i = 0; i < cmd_len; i++) {
        temp_name[5 + i] = p->source[cmd_tok->start + i];
    }
    uint32_t func_len = 5 + cmd_len;
    temp_name[func_len] = '\0';

    // Optional slash indicates arguments
    uint16_t arg_node = 0;
    if (check(p, TOK_SLASH) || check(p, TOK_DIV)) {
        advance(p); // consume slash
        // If next token is backslash, it's zero-arg form
        if (!check(p, TOK_BACKSLASH)) {
            // Parse first argument
            arg_node = parse_expression(p);
            // Check for optional second argument separated by comma
            if (match(p, TOK_COMMA)) {
                uint16_t second = parse_expression(p);
                if (second != 0 && arg_node != 0) {
                    uint16_t pair = alloc_node(p, NODE_BINARY_OP);
                    p->nodes[pair].data.binary.op = TOK_COMMA;
                    p->nodes[pair].data.binary.left_idx = arg_node;
                    p->nodes[pair].data.binary.right_idx = second;
                    arg_node = pair;
                }
            }
        }
        // Consume trailing backslash if present
        if (check(p, TOK_BACKSLASH)) {
            advance(p);
        }
    }

    // Create function call node
    uint16_t call_node = alloc_node(p, NODE_FUNC_CALL);
    if (call_node == 0) return 0;
    uint16_t name_node = alloc_node(p, NODE_IDENTIFIER);
    if (name_node == 0) return 0;

    // Store function name in string pool
    uint32_t name_offset = p->string_pos;
    for (uint32_t i = 0; i < func_len; i++) {
        p->string_pool[p->string_pos++] = temp_name[i];
    }
    p->string_pool[p->string_pos++] = '\0';

    p->nodes[name_node].data.ident.name_offset = name_offset;
    p->nodes[name_node].data.ident.name_len = func_len;

    p->nodes[call_node].data.binary.left_idx = name_node;
    p->nodes[call_node].data.binary.right_idx = arg_node;

    return call_node;
}

/* DUPLICATE SNIPPET DISABLED */
//// ... existing code ...
//    // GGGX generic: identifier 'gggx' followed by '.'
//    if (check(p, TOK_IDENTIFIER)) {
//        Token* id_tok = peek(p);
//        if (id_tok && id_tok->len == 4 && strncmp(p->source + id_tok->start, "gggx", 4) == 0) {
//            Token* dot_tok = peek2(p);
//            if (dot_tok && dot_tok->type == TOK_DOT) {
//                return parse_gggx_generic_command(p);
//            }
//        }
//    }
// ... existing code ...