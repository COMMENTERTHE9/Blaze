// BLAZE PARSER V2 - Handles new Blaze syntax
// Recursive descent parser for Blaze language constructs

#include "blaze_internals.h"

// Forward declare write
ssize_t write(int fd, const void* buf, size_t count);

// Parser state is now defined in blaze_internals.h

// Forward declarations
static uint16_t parse_expression(Parser* p);
static uint16_t parse_statement(Parser* p);
static uint16_t parse_block(Parser* p);

// Parser utilities
static inline bool at_end(Parser* p) {
    return p->current >= p->count || p->tokens[p->current].type == TOK_EOF;
}

static inline Token* peek(Parser* p) {
    if (at_end(p)) return NULL;
    return &p->tokens[p->current];
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
        p->has_error = true;
        return 0xFFFF; // Use 0xFFFF as error value instead of 0
    }
    
    uint16_t idx = p->node_count++;
    ASTNode* node = &p->nodes[idx];
    node->type = type;
    
    // Zero out data union
    uint64_t* data = (uint64_t*)&node->data;
    data[0] = 0;
    data[1] = 0;
    
    return idx;
}

// Store string in pool
static uint32_t store_string(Parser* p, Token* tok) {
    uint32_t offset = p->string_pos;
    
    // Copy token text to string pool
    for (uint16_t i = 0; i < tok->len; i++) {
        if (p->string_pos >= 4096) { // String pool limit
            p->has_error = true;
            return 0;
        }
        p->string_pool[p->string_pos++] = p->source[tok->start + i];
    }
    
    // Null terminate
    p->string_pool[p->string_pos++] = '\0';
    
    return offset;
}

// Extract identifier from complex token
static uint32_t extract_identifier(Parser* p, Token* tok, uint32_t skip_prefix) {
    uint32_t offset = p->string_pos;
    
    // Skip prefix and extract identifier part
    uint32_t start = tok->start + skip_prefix;
    uint32_t end = tok->start + tok->len;
    
    // Find where identifier ends (at '-' or '[')
    for (uint32_t i = start; i < end; i++) {
        char c = p->source[i];
        if (c == '-' || c == '[') {
            end = i;
            break;
        }
    }
    
    // Copy identifier
    for (uint32_t i = start; i < end; i++) {
        if (p->string_pos >= 4096) {
            p->has_error = true;
            return 0;
        }
        p->string_pool[p->string_pos++] = p->source[i];
    }
    
    p->string_pool[p->string_pos++] = '\0';
    return offset;
}

// Parse number literal
static uint16_t parse_number(Parser* p) {
    Token* num_tok = advance(p);
    uint16_t node_idx = alloc_node(p, NODE_NUMBER);
    
    if (node_idx == 0xFFFF) return 0xFFFF;
    
    // Convert string to number
    int64_t value = 0;
    bool negative = false;
    uint32_t i = 0;
    
    if (num_tok->len > 0 && p->source[num_tok->start] == '-') {
        negative = true;
        i = 1;
    }
    
    for (; i < num_tok->len; i++) {
        char c = p->source[num_tok->start + i];
        if (c >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
        }
    }
    
    if (negative) value = -value;
    
    p->nodes[node_idx].data.number = value;
    return node_idx;
}

// Parse identifier
static uint16_t parse_identifier(Parser* p) {
    Token* id_tok = advance(p);
    uint16_t node_idx = alloc_node(p, NODE_IDENTIFIER);
    
    if (node_idx == 0xFFFF) return 0xFFFF;
    
    uint32_t str_offset = store_string(p, id_tok);
    p->nodes[node_idx].data.ident.name_offset = str_offset;
    p->nodes[node_idx].data.ident.name_len = id_tok->len;
    
    return node_idx;
}

// Parse parameter: /{@param:name}
static uint16_t parse_parameter(Parser* p) {
    Token* param_tok = advance(p);
    uint16_t node_idx = alloc_node(p, NODE_IDENTIFIER);
    
    if (node_idx == 0xFFFF) return 0xFFFF;
    
    // Extract parameter name after @param:
    uint32_t name_start = 0;
    for (uint32_t i = 0; i < param_tok->len; i++) {
        if (p->source[param_tok->start + i] == ':') {
            name_start = i + 1;
            break;
        }
    }
    
    // Store parameter name
    uint32_t offset = p->string_pos;
    for (uint32_t i = name_start; i < param_tok->len - 1; i++) { // -1 to skip closing }
        if (p->string_pos >= 4096) {
            p->has_error = true;
            return 0;
        }
        p->string_pool[p->string_pos++] = p->source[param_tok->start + i];
    }
    p->string_pool[p->string_pos++] = '\0';
    
    p->nodes[node_idx].data.ident.name_offset = offset;
    p->nodes[node_idx].data.ident.name_len = param_tok->len - name_start - 1;
    
    return node_idx;
}

// Parse matrix: [:::name1-name2-name3[val1;val2;...]]
static uint16_t parse_matrix(Parser* p) {
    Token* matrix_tok = advance(p); // consume TOK_MATRIX_START
    
    uint16_t matrix_node = alloc_node(p, NODE_ARRAY_4D);
    if (matrix_node == 0xFFFF) return 0xFFFF;
    
    // Extract the entire matrix content from the token
    // The lexer gives us the whole matrix as one token
    uint32_t content_start = matrix_tok->start + 4; // Skip "[:::"
    uint32_t content_end = matrix_tok->start + matrix_tok->len - 1; // Skip final "]"
    
    // Parse dimension names (everything before the inner '[')
    uint32_t inner_bracket_pos = content_start;
    while (inner_bracket_pos < content_end && p->source[inner_bracket_pos] != '[') {
        inner_bracket_pos++;
    }
    
    // Store dimension names in string pool
    uint32_t dims_offset = p->string_pos;
    for (uint32_t i = content_start; i < inner_bracket_pos; i++) {
        if (p->string_pos >= 4096) {
            p->has_error = true;
            return 0xFFFF;
        }
        p->string_pool[p->string_pos++] = p->source[i];
    }
    p->string_pool[p->string_pos++] = '\0';
    
    // Store dimension info
    p->nodes[matrix_node].data.array_4d.name_idx = dims_offset;
    
    // TODO: Parse the values inside the inner brackets
    // For now, we just store the structure
    
    return matrix_node;
}

// Parse c.split._[description_count]
static uint16_t parse_split(Parser* p) {
    Token* split_tok = advance(p); // consume TOK_C_SPLIT
    
    uint16_t split_node = alloc_node(p, NODE_GAP_COMPUTE);
    if (split_node == 0xFFFF) return 0xFFFF;
    
    // Extract content from brackets
    uint32_t bracket_start = 0;
    for (uint32_t i = 0; i < split_tok->len; i++) {
        if (p->source[split_tok->start + i] == '[') {
            bracket_start = i + 1;
            break;
        }
    }
    
    if (bracket_start > 0) {
        // Find end of description and count
        uint32_t desc_end = split_tok->start + split_tok->len - 1; // Skip ']'
        
        // Extract count from end (number, null, none, all, ever)
        uint32_t count_start = desc_end - 1;
        while (count_start > split_tok->start + bracket_start) {
            char c = p->source[count_start];
            if (c == '_') {
                break;
            }
            count_start--;
        }
        
        // Store description in string pool
        uint32_t desc_offset = p->string_pos;
        for (uint32_t i = split_tok->start + bracket_start; i < count_start; i++) {
            if (p->string_pos >= 4096) {
                p->has_error = true;
                return 0xFFFF;
            }
            p->string_pool[p->string_pos++] = p->source[i];
        }
        p->string_pool[p->string_pos++] = '\0';
        
        p->nodes[split_node].data.gap_compute.var_idx = desc_offset;
        
        // TODO: Parse and store the count value
    }
    
    return split_node;
}

// Parse variable definition: var.v-name-[value]
static uint16_t parse_var_def(Parser* p) {
    Token* var_tok = advance(p); // consume TOK_VAR
    write(1, "In parse_var_def\n", 17);
    
    uint16_t var_node = alloc_node(p, NODE_VAR_DEF);
    if (var_node == 0xFFFF) return 0xFFFF;
    
    // Extract variable name from token
    uint32_t name_offset = extract_identifier(p, var_tok, 6); // Skip "var.v-"
    if (p->has_error) {
        return 0xFFFF;
    }
    p->nodes[var_node].data.ident.name_offset = name_offset;
    
    // Calculate name length
    uint32_t name_start = 6; // After "var.v-"
    uint32_t name_end = var_tok->len;
    for (uint32_t i = name_start; i < var_tok->len; i++) {
        if (p->source[var_tok->start + i] == '-' || p->source[var_tok->start + i] == '[') {
            name_end = i;
            break;
        }
    }
    uint32_t name_len = name_end - name_start;
    p->nodes[var_node].data.ident.name_len = name_len;
    
    // Debug: print next token type
    if (p->current < p->count) {
        write(1, "Next token type: ", 17);
        char buf[20];
        int len = 0;
        uint8_t tt = p->tokens[p->current].type;
        do {
            buf[len++] = '0' + (tt % 10);
            tt /= 10;
        } while (tt > 0 && len < 19);
        while (len > 0) write(1, &buf[--len], 1);
        write(1, "\n", 1);
    }
    
    // Check if there's an initialization value
    if (check(p, TOK_BRACKET_OPEN)) {
        advance(p); // consume [
        
        // Parse initialization expression
        uint16_t init_expr = parse_expression(p);
        
        // Store init expression in high bits of name_len (hacky but works)
        p->nodes[var_node].data.ident.name_len = name_len | (init_expr << 16);
        
        if (!match(p, TOK_BRACKET_CLOSE)) {
            p->has_error = true;
            if (p->current < p->count) {
                p->error_pos = p->tokens[p->current].start;
            }
            return 0;
        }
    }
    
    return var_node;
}

// Parse function definition: |function_name| method.can< params< :>
static uint16_t parse_func_def(Parser* p) {
    advance(p); // consume opening |
    
    uint16_t func_node = alloc_node(p, NODE_FUNC_DEF);
    if (func_node == 0xFFFF) return 0xFFFF;
    
    // Function name
    if (check(p, TOK_IDENTIFIER)) {
        Token* name_tok = advance(p);
        uint32_t name_offset = store_string(p, name_tok);
        p->nodes[func_node].data.timing.expr_idx = name_offset;
    }
    
    if (!match(p, TOK_PIPE)) {
        p->has_error = true;
        return 0;
    }
    
    // Check for method.can< syntax
    if (check(p, TOK_IDENTIFIER)) {
        Token* method = peek(p);
        // Check if it starts with "method."
        if (method->len > 7 && p->source[method->start] == 'm') {
            advance(p);
            
            // Parse parameters if < follows
            if (match(p, TOK_LT)) {
                // Parse parameter list
                while (!at_end(p) && !check(p, TOK_FUNC_CLOSE)) {
                    if (check(p, TOK_PARAM)) {
                        parse_parameter(p);
                    } else {
                        advance(p);
                    }
                }
            }
        }
    }
    
    // Parse function body
    if (match(p, TOK_FUNC_CLOSE)) {
        // Function has a body
        uint16_t body = parse_block(p);
        p->nodes[func_node].data.timing.temporal_offset = body;
    }
    
    return func_node;
}

// Parse conditional: f.ens, f.ver, etc.
static uint16_t parse_conditional(Parser* p) {
    Token* cond_tok = advance(p);
    
    uint16_t cond_node = alloc_node(p, NODE_CONDITIONAL);
    if (cond_node == 0xFFFF) return 0xFFFF;
    
    // Store conditional type
    p->nodes[cond_node].data.binary.op = cond_tok->type;
    
    // Parse condition parameters
    if (match(p, TOK_SLASH)) {
        uint16_t param = parse_expression(p);
        p->nodes[cond_node].data.binary.left_idx = param;
    }
    
    // Parse condition body
    if (match(p, TOK_CONNECTOR_FWD) || match(p, TOK_CONNECTOR_BWD)) {
        uint16_t body = parse_statement(p);
        p->nodes[cond_node].data.binary.right_idx = body;
    }
    
    return cond_node;
}

// Parse timeline: timeline-[name] or ^timeline.[target bnc timeline recv]/
static uint16_t parse_timeline(Parser* p) {
    Token* timeline_tok = advance(p);
    
    uint16_t timeline_node = alloc_node(p, NODE_JUMP);
    if (timeline_node == 0xFFFF) return 0xFFFF;
    
    if (timeline_tok->type == TOK_TIMELINE_DEF) {
        // Simple timeline definition: timeline-[name]
        // Parse until closing bracket
        while (!at_end(p) && !check(p, TOK_BRACKET_CLOSE)) {
            advance(p);
        }
        match(p, TOK_BRACKET_CLOSE);
    }
    else if (timeline_tok->type == TOK_TIMELINE_JUMP) {
        // Timeline jump: ^timeline.[target bnc timeline recv]/
        
        // Parse target expression (e.g., |processor|.state_1 or just state_1)
        uint16_t target = 0xFFFF;
        if (check(p, TOK_PIPE)) {
            // Function-style target: |processor|.state_1
            advance(p); // consume |
            if (check(p, TOK_IDENTIFIER)) {
                advance(p); // consume processor
            }
            match(p, TOK_PIPE); // consume |
            match(p, TOK_DOT); // consume .
        }
        
        if (check(p, TOK_IDENTIFIER)) {
            target = parse_identifier(p);
        }
        
        p->nodes[timeline_node].data.timing.expr_idx = target;
        
        // Check for bounce syntax
        if (check(p, TOK_BNC)) {
            advance(p); // consume 'bnc'
            
            // Parse timeline to bounce
            if (check(p, TOK_IDENTIFIER)) {
                uint16_t bounce_target = parse_identifier(p);
                p->nodes[timeline_node].data.timing.temporal_offset = bounce_target;
            }
            
            // Expect 'recv'
            if (!match(p, TOK_RECV)) {
                p->has_error = true;
                return 0xFFFF;
            }
            
            // Mark this as a bounce operation
            p->nodes[timeline_node].data.timing.timing_op = TOK_BNC;
        }
        
        // Expect closing ]
        if (!match(p, TOK_BRACKET_CLOSE)) {
            p->has_error = true;
            return 0xFFFF;
        }
        
        // Expect /
        if (!match(p, TOK_SLASH)) {
            p->has_error = true;
            return 0xFFFF;
        }
    }
    
    return timeline_node;
}

// Parse fixed point: fix.p-[name] or f.p-[name] or inline f.p
static uint16_t parse_fixed_point(Parser* p) {
    Token* fp_tok = advance(p); // consume TOK_FIX_P or TOK_F_P
    
    uint16_t fp_node = alloc_node(p, NODE_FIXED_POINT);
    if (fp_node == 0xFFFF) return 0xFFFF;
    
    // Check if this is a definition with bracket
    if (check(p, TOK_BRACKET_OPEN)) {
        advance(p); // consume [
        
        if (check(p, TOK_IDENTIFIER)) {
            uint16_t name = parse_identifier(p);
            p->nodes[fp_node].data.fixed_point.name_idx = name;
        }
        
        match(p, TOK_BRACKET_CLOSE); // consume ]
    }
    else if (check(p, TOK_DOT) && !at_end(p)) {
        // Inline fixed point reference like f.p.checkpoint
        advance(p); // consume .
        if (check(p, TOK_IDENTIFIER)) {
            uint16_t name = parse_identifier(p);
            p->nodes[fp_node].data.fixed_point.name_idx = name;
        }
    }
    
    return fp_node;
}

// Parse permanent timeline: timelineper-[name] or timelinep-[name]
static uint16_t parse_permanent_timeline(Parser* p) {
    Token* timeline_tok = advance(p);
    
    uint16_t timeline_node = alloc_node(p, NODE_PERMANENT_TIMELINE);
    if (timeline_node == 0xFFFF) return 0xFFFF;
    
    // For timeline definition
    if (timeline_tok->type == TOK_TIMELINE_PER || timeline_tok->type == TOK_TIMELINE_P) {
        // Parse until closing bracket
        while (!at_end(p) && !check(p, TOK_BRACKET_CLOSE)) {
            advance(p);
        }
        match(p, TOK_BRACKET_CLOSE);
        
        // Check for flow specification like .{@rate:60}
        if (check(p, TOK_DOT)) {
            advance(p); // consume .
            if (check(p, TOK_LBRACE)) {
                advance(p); // consume {
                
                // Create flow spec node
                uint16_t flow_node = alloc_node(p, NODE_FLOW_SPEC);
                if (flow_node != 0xFFFF) {
                    p->nodes[flow_node].data.flow_spec.timeline_idx = timeline_node;
                    p->nodes[flow_node].data.flow_spec.flow_type = 0; // PERMANENT
                    
                    // Parse rate if present
                    if (check(p, TOK_AT)) {
                        advance(p); // consume @
                        if (check(p, TOK_IDENTIFIER)) {
                            Token* rate_tok = advance(p); // should be "rate"
                            if (match(p, TOK_COLON) && check(p, TOK_NUMBER)) {
                                Token* num = advance(p);
                                // Extract rate value
                                uint16_t rate = 0;
                                for (uint32_t i = 0; i < num->len; i++) {
                                    rate = rate * 10 + (p->source[num->start + i] - '0');
                                }
                                p->nodes[flow_node].data.flow_spec.rate = rate;
                            }
                        }
                    }
                    
                    match(p, TOK_RBRACE); // consume }
                    return flow_node;
                }
            }
        }
    }
    // For timeline jump ^timelinep.[
    else if (timeline_tok->type == TOK_TIMELINE_P_JUMP) {
        // Parse the jump target
        if (check(p, TOK_PIPE)) {
            // Function-style target
            advance(p); // consume |
            if (check(p, TOK_IDENTIFIER)) {
                uint16_t target = parse_identifier(p);
                p->nodes[timeline_node].data.timing.expr_idx = target;
            }
            match(p, TOK_PIPE); // consume |
        }
        
        match(p, TOK_BRACKET_CLOSE); // consume ]
        match(p, TOK_SLASH); // consume /
    }
    
    return timeline_node;
}

// Parse string literal
static uint16_t parse_string(Parser* p) {
    Token* str_tok = advance(p); // consume string token
    
    uint16_t str_node = alloc_node(p, NODE_STRING);
    if (str_node == 0xFFFF) return 0xFFFF;
    
    // Copy string content to string pool (without quotes)
    uint32_t offset = p->string_pos;
    uint32_t start = str_tok->start + 1; // Skip opening quote
    uint32_t end = str_tok->start + str_tok->len - 1; // Skip closing quote
    
    for (uint32_t i = start; i < end && p->string_pos < 4095; i++) {
        p->string_pool[p->string_pos++] = p->source[i];
    }
    p->string_pool[p->string_pos++] = '\0';
    
    p->nodes[str_node].data.ident.name_offset = offset;
    return str_node;
}

// Parse output: print/"text"\ or out/"expr"\ etc.
static uint16_t parse_output(Parser* p) {
    Token* output_tok = advance(p); // consume output method token
    
    uint16_t output_node = alloc_node(p, NODE_OUTPUT);
    if (output_node == 0xFFFF) return 0xFFFF;
    
    p->nodes[output_node].data.output.output_type = output_tok->type;
    p->nodes[output_node].data.output.next_output = 0xFFFF;
    
    // Parse the content expression
    if (check(p, TOK_IDENTIFIER)) {
        // Parse identifier (variable reference)
        uint16_t id_node = parse_identifier(p);
        p->nodes[output_node].data.output.content_idx = id_node;
    } else if (check(p, TOK_NUMBER)) {
        // Parse number
        uint16_t num_node = parse_number(p);
        p->nodes[output_node].data.output.content_idx = num_node;
    } else if (check(p, TOK_STRING)) {
        // Parse string
        uint16_t str_node = parse_string(p);
        p->nodes[output_node].data.output.content_idx = str_node;
    } else {
        // For now, just skip to the ending backslash for unknown content
        while (!at_end(p) && !check(p, TOK_BACKSLASH)) {
            advance(p);
        }
        p->nodes[output_node].data.output.content_idx = 0xFFFF;
    }
    
    // Consume ending backslash
    if (check(p, TOK_BACKSLASH)) {
        advance(p);
    } else {
        // DEBUG: Print what token we found instead
        print_str("Expected backslash after print, found token: ");
        if (peek(p)) {
            print_num(peek(p)->type);
            print_str(" at position ");
            print_num(peek(p)->start);
        } else {
            print_str("EOF");
        }
        print_str("\n");
    }
    
    return output_node;
}

// Parse action block: do/ ... / ... backslash
static uint16_t parse_action_block(Parser* p) {
    advance(p); // consume TOK_ACTION_START
    
    uint16_t action_node = alloc_node(p, NODE_ACTION_BLOCK);
    if (action_node == 0xFFFF) return 0xFFFF;
    
    uint16_t first_action = 0;
    uint16_t last_action = 0;
    
    while (!at_end(p) && !check(p, TOK_BACKSLASH)) {
        uint16_t stmt = parse_statement(p);
        
        if (first_action == 0) {
            first_action = stmt;
            p->nodes[action_node].data.binary.left_idx = first_action;
        }
        
        // Chain actions
        if (last_action != 0) {
            p->nodes[last_action].data.binary.right_idx = stmt;
        }
        last_action = stmt;
        
        // Continue marker
        match(p, TOK_SLASH);
    }
    
    match(p, TOK_BACKSLASH); // Consume ending backslash
    
    return action_node;
}

// Parse expression
static uint16_t parse_expression(Parser* p) {
    // Parameters
    if (check(p, TOK_PARAM)) {
        return parse_parameter(p);
    }
    
    // Numbers
    if (check(p, TOK_NUMBER)) {
        return parse_number(p);
    }
    
    // Identifiers
    if (check(p, TOK_IDENTIFIER)) {
        return parse_identifier(p);
    }
    
    // Matrix
    if (check(p, TOK_MATRIX_START)) {
        return parse_matrix(p);
    }
    
    // Split operations
    if (check(p, TOK_C_SPLIT)) {
        return parse_split(p);
    }
    
    // Fixed points in expressions
    if (check(p, TOK_F_P) || check(p, TOK_FIX_P)) {
        return parse_fixed_point(p);
    }
    
    // Check for recv._merg or recv._queue
    if (check(p, TOK_RECV)) {
        uint32_t saved_pos = p->current;
        advance(p); // consume 'recv'
        
        if (match(p, TOK_DOT)) {
            if (check(p, TOK_IDENTIFIER)) {
                Token* method = peek(p);
                bool is_merge = (method->len == 5 && p->source[method->start] == '_' && 
                                p->source[method->start + 1] == 'm');
                bool is_queue = (method->len == 6 && p->source[method->start] == '_' && 
                                p->source[method->start + 1] == 'q');
                
                if (is_merge || is_queue) {
                    advance(p); // consume _merg or _queue
                    
                    uint16_t recv_node = alloc_node(p, NODE_TIMING_OP);
                    if (recv_node == 0xFFFF) return 0xFFFF;
                    
                    // Mark type: RECV for merge, RECV+1 for queue
                    p->nodes[recv_node].data.timing.timing_op = is_merge ? TOK_RECV : (TOK_RECV + 1);
                    
                    // Parse parameters if present
                    if (check(p, TOK_SLASH) || check(p, TOK_PARAM)) {
                        // Parameters follow
                    }
                    
                    return recv_node;
                }
            }
        }
        
        // Not recv._merg or recv._queue, backtrack
        p->current = saved_pos;
        return parse_identifier(p); // Just treat 'recv' as identifier
    }
    
    // Temporal operators
    if (check(p, TOK_LT) || check(p, TOK_GT) ||
        check(p, TOK_TIMING_ONTO) || check(p, TOK_TIMING_INTO) || check(p, TOK_TIMING_BOTH)) {
        Token* op = advance(p);
        uint16_t timing_node = alloc_node(p, NODE_TIMING_OP);
        p->nodes[timing_node].data.timing.timing_op = op->type;
        p->nodes[timing_node].data.timing.expr_idx = parse_expression(p);
        return timing_node;
    }
    
    p->has_error = true;
    if (p->current < p->count) {
        p->error_pos = p->tokens[p->current].start;
    }
    return 0xFFFF;
}

// Parse function call: ^function_name/{@param:arg1}/{@param:arg2}/\
static uint16_t parse_function_call(Parser* p) {
    Token* func_tok = advance(p); // Consume ^function_name
    
    // Extract function name (skip the ^)
    uint32_t name_offset = store_string(p, func_tok) + 1; // +1 to skip ^
    
    // Create function call node
    uint16_t call_idx = alloc_node(p, NODE_FUNC_CALL);
    if (call_idx == 0xFFFF) return 0xFFFF;
    
    ASTNode* call_node = &p->nodes[call_idx];
    call_node->data.ident.name_offset = name_offset;
    call_node->data.ident.name_len = func_tok->len - 1; // -1 for ^
    
    // Parse parameters
    uint16_t first_param = 0xFFFF;
    uint16_t last_param = 0xFFFF;
    
    while (match(p, TOK_SLASH)) {
        // Check for parameter
        if (check(p, TOK_PARAM)) {
            Token* param_tok = advance(p);
            
            // Create parameter node
            uint16_t param_idx = alloc_node(p, NODE_IDENTIFIER);
            if (param_idx == 0xFFFF) return 0xFFFF;
            
            // Store parameter name
            uint32_t param_offset = store_string(p, param_tok);
            p->nodes[param_idx].data.ident.name_offset = param_offset;
            p->nodes[param_idx].data.ident.name_len = param_tok->len;
            
            // Link parameters
            if (first_param == 0xFFFF) {
                first_param = param_idx;
            } else {
                p->nodes[last_param].data.binary.right_idx = param_idx;
            }
            last_param = param_idx;
        } else {
            // Parse expression as parameter
            uint16_t expr = parse_expression(p);
            if (expr == 0xFFFF) return 0xFFFF;
            
            // Link parameters
            if (first_param == 0xFFFF) {
                first_param = expr;
            } else {
                p->nodes[last_param].data.binary.right_idx = expr;
            }
            last_param = expr;
        }
    }
    
    // Expect closing backslash
    if (!match(p, TOK_BACKSLASH)) {
        p->has_error = true;
        return 0xFFFF;
    }
    
    // Store parameters in call node
    call_node->data.binary.left_idx = first_param;
    
    return call_idx;
}

// Parse block of statements
static uint16_t parse_block(Parser* p) {
    uint16_t first_stmt = 0;
    uint16_t last_stmt = 0;
    
    while (!at_end(p) && !check(p, TOK_FUNC_CLOSE) && !check(p, TOK_BACKSLASH)) {
        uint16_t stmt = parse_statement(p);
        
        if (p->has_error) break;
        
        if (first_stmt == 0) {
            first_stmt = stmt;
        }
        
        if (last_stmt != 0) {
            p->nodes[last_stmt].data.binary.right_idx = stmt;
        }
        last_stmt = stmt;
    }
    
    return first_stmt;
}

// Parse statement
static uint16_t parse_statement(Parser* p) {
    // Check for conditional abbreviations
    Token* tok = peek(p);
    if (tok && tok->type >= TOK_COND_ENS && tok->type <= TOK_COND_MSR) {
        // Parse conditional: f.xxx/param\>|body
        advance(p); // Consume conditional token
        
        // Expect / for parameter
        if (!match(p, TOK_SLASH)) {
            p->has_error = true;
            return 0xFFFF;
        }
        
        // Parse parameter expression
        uint16_t param_idx = parse_expression(p);
        if (param_idx == 0xFFFF) {
            return 0xFFFF;
        }
        
        // Expect \>| connector
        if (!match(p, TOK_CONNECTOR_FWD)) {
            p->has_error = true;
            return 0xFFFF;
        }
        
        // Parse body statement
        uint16_t body_idx = parse_statement(p);
        if (body_idx == 0xFFFF) {
            return 0xFFFF;
        }
        
        // Create conditional node
        uint16_t node_idx = alloc_node(p, NODE_CONDITIONAL);
        if (node_idx == 0xFFFF) {
            return 0xFFFF;
        }
        
        ASTNode* node = &p->nodes[node_idx];
        node->data.binary.op = tok->type; // Store the specific conditional type
        node->data.binary.left_idx = param_idx;
        node->data.binary.right_idx = body_idx;
        
        return node_idx;
    }
    
    // Variable definition
    if (check(p, TOK_VAR)) {
        return parse_var_def(p);
    }
    
    // Function definition
    if (check(p, TOK_PIPE)) {
        return parse_func_def(p);
    }
    
    // Conditionals
    if (check(p, TOK_COND_ENS) || check(p, TOK_COND_VER) || 
        check(p, TOK_COND_CHK) || check(p, TOK_COND_TRY) ||
        check(p, TOK_COND_GRD) || check(p, TOK_COND_UNL) ||
        check(p, TOK_COND_IF) || check(p, TOK_COND_WHL) ||
        check(p, TOK_COND_UNT) || check(p, TOK_COND_OBS) ||
        check(p, TOK_COND_DET) || check(p, TOK_COND_REC) ||
        check(p, TOK_COND_FS) || check(p, TOK_COND_RTE) ||
        check(p, TOK_COND_MON) || check(p, TOK_COND_EVAL) ||
        check(p, TOK_COND_DEC) || check(p, TOK_COND_ASS) ||
        check(p, TOK_COND_MSR)) {
        return parse_conditional(p);
    }
    
    // Timeline
    if (check(p, TOK_TIMELINE_DEF) || check(p, TOK_TIMELINE_JUMP)) {
        return parse_timeline(p);
    }
    
    // Fixed points
    if (check(p, TOK_FIX_P) || check(p, TOK_F_P)) {
        return parse_fixed_point(p);
    }
    
    // Permanent timelines
    if (check(p, TOK_TIMELINE_PER) || check(p, TOK_TIMELINE_P) || check(p, TOK_TIMELINE_P_JUMP)) {
        return parse_permanent_timeline(p);
    }
    
    // Output methods
    if (check(p, TOK_PRINT) || check(p, TOK_TXT) || check(p, TOK_OUT) || 
        check(p, TOK_FMT) || check(p, TOK_DYN)) {
        return parse_output(p);
    }
    
    // Function call (^function_name)
    if (check(p, TOK_FUNC_CALL)) {
        return parse_function_call(p);
    }
    
    // Action block
    if (check(p, TOK_ACTION_START)) {
        return parse_action_block(p);
    }
    
    // Bounce
    if (check(p, TOK_BNC)) {
        advance(p);
        uint16_t bnc_node = alloc_node(p, NODE_JUMP);
        p->nodes[bnc_node].data.timing.timing_op = TOK_BNC;
        return bnc_node;
    }
    
    // Expression statement
    uint16_t expr = parse_expression(p);
    
    // If expression failed and we haven't advanced, skip this token to avoid infinite loop
    if (expr == 0xFFFF && peek(p) && peek(p)->type != TOK_EOF) {
        // Print debug info about the problematic token
        print_str("Warning: Skipping unknown token type=");
        print_num(peek(p)->type);
        print_str(" at position ");
        print_num(peek(p)->start);
        print_str("\n");
        advance(p); // Skip the problematic token
        return 0xFFFF;
    }
    
    return expr;
}

// Main parse function
uint16_t parse_blaze_v2(Token* tokens, uint32_t count, ASTNode* node_pool, 
                        uint32_t pool_size, char* string_pool, const char* source) {
    Parser parser = {
        .tokens = tokens,
        .count = count,
        .current = 0,
        .nodes = node_pool,
        .node_count = 0,
        .node_capacity = pool_size,
        .string_pool = string_pool,
        .string_pos = 0,
        .source = source,
        .has_error = false,
        .error_pos = 0
    };
    
    // Create root program node
    uint16_t program_node = alloc_node(&parser, NODE_PROGRAM);
    if (program_node == 0xFFFF) return 0;
    
    // Parse all statements
    uint16_t first_stmt = 0;
    uint16_t last_stmt = 0;
    
    while (!at_end(&parser)) {
        uint16_t stmt = parse_statement(&parser);
        
        if (parser.has_error || stmt == 0xFFFF) {
            return 0xFFFF; // Parse failed
        }
        
        if (first_stmt == 0 || first_stmt == 0xFFFF) {
            first_stmt = stmt;
            parser.nodes[program_node].data.binary.left_idx = first_stmt;
        }
        
        // Chain statements
        if (last_stmt != 0 && last_stmt < pool_size) {
            parser.nodes[last_stmt].data.binary.right_idx = stmt;
        }
        last_stmt = stmt;
    }
    
    return program_node;
}