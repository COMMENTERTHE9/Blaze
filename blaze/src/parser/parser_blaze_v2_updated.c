// BLAZE PARSER V2 - Recursive descent parser with time-travel support
// Updated to support conditional abbreviations

#include "blaze_internals.h"

// Parser state
typedef struct {
    Token* tokens;
    uint32_t count;
    uint32_t pos;
    ASTNode* nodes;
    uint32_t node_count;
    uint32_t node_capacity;
    char* string_pool;
    uint32_t string_pos;
    const char* source;
    bool has_error;
} Parser;

// Forward declarations
static uint16_t parse_statement(Parser* p);
static uint16_t parse_expression(Parser* p);
static uint16_t parse_block(Parser* p);

// Utility functions
static bool at_end(Parser* p) {
    return p->pos >= p->count || p->tokens[p->pos].type == TOK_EOF;
}

static Token* peek(Parser* p) {
    if (at_end(p)) return NULL;
    return &p->tokens[p->pos];
}

static Token* advance(Parser* p) {
    if (!at_end(p)) p->pos++;
    return peek(p);
}

static bool check(Parser* p, TokenType type) {
    if (at_end(p)) return false;
    return p->tokens[p->pos].type == type;
}

static bool match(Parser* p, TokenType type) {
    if (check(p, type)) {
        advance(p);
        return true;
    }
    return false;
}

static uint16_t alloc_node(Parser* p, NodeType type) {
    if (p->node_count >= p->node_capacity) {
        p->has_error = true;
        return 0xFFFF;
    }
    
    uint16_t idx = p->node_count++;
    ASTNode* node = &p->nodes[idx];
    node->type = type;
    
    // Initialize to safe defaults
    node->data.number = 0;
    
    return idx;
}

static uint32_t store_string(Parser* p, const char* str, uint32_t len) {
    if (p->string_pos + len + 1 >= 4096) {
        p->has_error = true;
        return 0;
    }
    
    uint32_t offset = p->string_pos;
    for (uint32_t i = 0; i < len; i++) {
        p->string_pool[p->string_pos++] = str[i];
    }
    p->string_pool[p->string_pos++] = '\0';
    
    return offset;
}

// Extract identifier from token
static uint32_t extract_identifier(Parser* p, Token* tok) {
    return store_string(p, &p->source[tok->start], tok->len);
}

// Parse number literal
static uint16_t parse_number(Parser* p) {
    Token* tok = peek(p);
    if (!tok || tok->type != TOK_NUMBER) return 0xFFFF;
    
    advance(p);
    
    uint16_t idx = alloc_node(p, NODE_NUMBER);
    if (idx == 0xFFFF) return 0xFFFF;
    
    // Parse number from source
    int64_t value = 0;
    for (uint32_t i = 0; i < tok->len; i++) {
        char c = p->source[tok->start + i];
        if (c >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
        }
    }
    
    p->nodes[idx].data.number = value;
    return idx;
}

// Parse identifier
static uint16_t parse_identifier(Parser* p) {
    Token* tok = peek(p);
    if (!tok || tok->type != TOK_IDENTIFIER) return 0xFFFF;
    
    advance(p);
    
    uint16_t idx = alloc_node(p, NODE_IDENTIFIER);
    if (idx == 0xFFFF) return 0xFFFF;
    
    p->nodes[idx].data.ident.name_offset = extract_identifier(p, tok);
    p->nodes[idx].data.ident.name_len = tok->len;
    
    return idx;
}

// Parse parameter reference: @param:name
static uint16_t parse_parameter(Parser* p) {
    Token* tok = peek(p);
    if (!tok || tok->type != TOK_PARAM) return 0xFFFF;
    
    advance(p);
    
    uint16_t idx = alloc_node(p, NODE_IDENTIFIER);
    if (idx == 0xFFFF) return 0xFFFF;
    
    // Store the parameter name (skip @param: prefix)
    uint32_t offset = 7; // Length of "@param:"
    if (tok->start + offset < tok->start + tok->len) {
        p->nodes[idx].data.ident.name_offset = store_string(p, 
            &p->source[tok->start + offset], 
            tok->len - offset);
        p->nodes[idx].data.ident.name_len = tok->len - offset;
    }
    
    return idx;
}

// Parse matrix: [:::dimensions[values]]
static uint16_t parse_matrix(Parser* p) {
    Token* tok = peek(p);
    if (!tok || tok->type != TOK_MATRIX_START) return 0xFFFF;
    
    advance(p);
    
    // For now, create a simple array node
    uint16_t idx = alloc_node(p, NODE_ARRAY_4D);
    if (idx == 0xFFFF) return 0xFFFF;
    
    // Store the entire matrix content as a string
    p->nodes[idx].data.array_4d.name_idx = extract_identifier(p, tok);
    
    return idx;
}

// Parse split operations: c.split._, cac._, Crack._
static uint16_t parse_split(Parser* p) {
    Token* tok = peek(p);
    if (!tok || tok->type != TOK_C_SPLIT) return 0xFFFF;
    
    advance(p);
    
    // Create a GAP compute node for split operations
    uint16_t idx = alloc_node(p, NODE_GAP_COMPUTE);
    if (idx == 0xFFFF) return 0xFFFF;
    
    // Store the split description
    p->nodes[idx].data.gap_compute.var_idx = extract_identifier(p, tok);
    
    return idx;
}

// Parse variable definition: var.v-name-[value]
static uint16_t parse_var_def(Parser* p) {
    Token* tok = peek(p);
    if (!tok || tok->type != TOK_VAR) return 0xFFFF;
    
    advance(p);
    
    uint16_t idx = alloc_node(p, NODE_VAR_DEF);
    if (idx == 0xFFFF) return 0xFFFF;
    
    // Extract variable name (between var.v- and optional -[)
    const char* src = &p->source[tok->start];
    uint32_t name_start = 6; // Skip "var.v-"
    uint32_t name_end = tok->len;
    
    // Find where name ends (at -[ or end of token)
    for (uint32_t i = name_start; i < tok->len - 1; i++) {
        if (src[i] == '-' && src[i+1] == '[') {
            name_end = i;
            break;
        }
    }
    
    // Store variable name
    uint32_t name_len = name_end - name_start;
    p->nodes[idx].data.ident.name_offset = store_string(p, &src[name_start], name_len);
    p->nodes[idx].data.ident.name_len = name_len;
    
    // Check for initialization value after current token
    if (check(p, TOK_BRACKET_OPEN)) {
        advance(p); // Skip [
        // Parse initialization value
        if (check(p, TOK_NUMBER)) {
            advance(p); // Skip number for now
        } else if (check(p, TOK_IDENTIFIER)) {
            advance(p); // Skip identifier for now
        }
        match(p, TOK_BRACKET_CLOSE); // Skip ]
    }
    
    return idx;
}

// Parse function definition: |name| or method.can<
static uint16_t parse_func_def(Parser* p) {
    Token* start_tok = peek(p);
    
    if (check(p, TOK_PIPE)) {
        // Function: |name|
        advance(p); // Skip |
        
        uint16_t name_idx = parse_identifier(p);
        if (name_idx == 0xFFFF) return 0xFFFF;
        
        if (!match(p, TOK_PIPE)) {
            p->has_error = true;
            return 0xFFFF;
        }
        
        uint16_t idx = alloc_node(p, NODE_FUNC_DEF);
        if (idx == 0xFFFF) return 0xFFFF;
        
        p->nodes[idx].data.binary.left_idx = name_idx;
        return idx;
    }
    else if (check(p, TOK_IDENTIFIER)) {
        // Could be method.can<
        Token* id_tok = peek(p);
        advance(p);
        
        if (match(p, TOK_DOT) && check(p, TOK_IDENTIFIER)) {
            Token* method_tok = peek(p);
            advance(p);
            
            if (match(p, TOK_LT)) {
                // This is a method definition
                uint16_t idx = alloc_node(p, NODE_FUNC_DEF);
                if (idx == 0xFFFF) return 0xFFFF;
                
                // Store method name
                p->nodes[idx].data.ident.name_offset = extract_identifier(p, method_tok);
                p->nodes[idx].data.ident.name_len = method_tok->len;
                
                return idx;
            }
        }
        
        // Not a function definition, backtrack
        p->pos = start_tok->start;
    }
    
    return 0xFFFF;
}

// Parse action block: do/ ... \
static uint16_t parse_action_block(Parser* p) {
    if (!check(p, TOK_ACTION_START)) return 0xFFFF;
    advance(p);
    
    uint16_t idx = alloc_node(p, NODE_ACTION_BLOCK);
    if (idx == 0xFFFF) return 0xFFFF;
    
    // Parse statements until we hit backslash or end
    uint16_t first_stmt = 0xFFFF;
    uint16_t last_stmt = 0xFFFF;
    
    while (!at_end(p) && !check(p, TOK_BACKSLASH)) {
        uint16_t stmt = parse_statement(p);
        if (stmt == 0xFFFF) break;
        
        if (first_stmt == 0xFFFF) {
            first_stmt = stmt;
            last_stmt = stmt;
        } else {
            // Chain statements
            if (last_stmt < p->node_capacity) {
                // Link statements together via right_idx
                p->nodes[last_stmt].data.binary.right_idx = stmt;
                last_stmt = stmt;
            }
        }
    }
    
    match(p, TOK_BACKSLASH); // Consume ending backslash if present
    
    p->nodes[idx].data.binary.left_idx = first_stmt;
    return idx;
}

// Parse timeline definition or jump
static uint16_t parse_timeline(Parser* p) {
    if (check(p, TOK_TIMELINE_DEF)) {
        // timeline-[
        advance(p);
        
        uint16_t idx = alloc_node(p, NODE_JUMP);
        if (idx == 0xFFFF) return 0xFFFF;
        
        // Parse timeline content
        // For now, just consume until ]
        while (!at_end(p) && !check(p, TOK_BRACKET_CLOSE)) {
            advance(p);
        }
        match(p, TOK_BRACKET_CLOSE);
        
        return idx;
    }
    else if (check(p, TOK_TIMELINE_JUMP)) {
        // ^timeline.[
        advance(p);
        
        uint16_t idx = alloc_node(p, NODE_JUMP);
        if (idx == 0xFFFF) return 0xFFFF;
        
        // Parse jump target
        while (!at_end(p) && !check(p, TOK_BRACKET_CLOSE)) {
            advance(p);
        }
        match(p, TOK_BRACKET_CLOSE);
        
        return idx;
    }
    
    return 0xFFFF;
}

// Parse conditional: f.xxx/param\>|body
static uint16_t parse_conditional(Parser* p) {
    Token* cond_tok = peek(p);
    
    // Check if this is a conditional token
    if (!cond_tok || cond_tok->type < TOK_COND_ENS || cond_tok->type > TOK_COND_MSR) {
        return 0xFFFF;
    }
    
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
    node->data.binary.op = cond_tok->type; // Store the specific conditional type
    node->data.binary.left_idx = param_idx;
    node->data.binary.right_idx = body_idx;
    
    return node_idx;
}

// Parse expression
static uint16_t parse_expression(Parser* p) {
    // Try various expression types
    uint16_t expr = 0xFFFF;
    
    if (check(p, TOK_NUMBER)) {
        expr = parse_number(p);
    }
    else if (check(p, TOK_IDENTIFIER)) {
        expr = parse_identifier(p);
    }
    else if (check(p, TOK_PARAM)) {
        expr = parse_parameter(p);
    }
    else if (check(p, TOK_MATRIX_START)) {
        expr = parse_matrix(p);
    }
    else if (check(p, TOK_C_SPLIT)) {
        expr = parse_split(p);
    }
    
    // Handle timing operators: <, >, <<, >>, <>
    if (expr != 0xFFFF) {
        if (check(p, TOK_LT) || check(p, TOK_GT) || 
            check(p, TOK_TIMING_ONTO) || check(p, TOK_TIMING_INTO) || 
            check(p, TOK_TIMING_BOTH)) {
            
            Token* op = peek(p);
            advance(p);
            
            uint16_t right = parse_expression(p);
            if (right != 0xFFFF) {
                uint16_t timing_idx = alloc_node(p, NODE_TIMING_OP);
                if (timing_idx != 0xFFFF) {
                    p->nodes[timing_idx].data.timing.timing_op = op->type;
                    p->nodes[timing_idx].data.timing.expr_idx = expr;
                    p->nodes[timing_idx].data.timing.temporal_offset = 0;
                    expr = timing_idx;
                }
            }
        }
    }
    
    return expr;
}

// Parse block of statements
static uint16_t parse_block(Parser* p) {
    uint16_t first = 0xFFFF;
    uint16_t last = 0xFFFF;
    
    while (!at_end(p)) {
        uint16_t stmt = parse_statement(p);
        if (stmt == 0xFFFF) break;
        
        if (first == 0xFFFF) {
            first = stmt;
            last = stmt;
        } else {
            // Chain statements
            if (last < p->node_capacity) {
                p->nodes[last].data.binary.right_idx = stmt;
                last = stmt;
            }
        }
    }
    
    return first;
}

// Parse statement
static uint16_t parse_statement(Parser* p) {
    // Check for conditionals first
    Token* tok = peek(p);
    if (tok && tok->type >= TOK_COND_ENS && tok->type <= TOK_COND_MSR) {
        return parse_conditional(p);
    }
    
    // Try variable definition
    if (check(p, TOK_VAR)) {
        return parse_var_def(p);
    }
    
    // Try function definition
    if (check(p, TOK_PIPE) || check(p, TOK_IDENTIFIER)) {
        uint16_t func = parse_func_def(p);
        if (func != 0xFFFF) return func;
    }
    
    // Try action block
    if (check(p, TOK_ACTION_START)) {
        return parse_action_block(p);
    }
    
    // Try timeline
    if (check(p, TOK_TIMELINE_DEF) || check(p, TOK_TIMELINE_JUMP)) {
        return parse_timeline(p);
    }
    
    // Try expression
    return parse_expression(p);
}

// Main parse function
uint16_t parse_blaze_v2(Token* tokens, uint32_t count, ASTNode* node_pool, 
                       uint32_t pool_size, char* string_pool, const char* source) {
    Parser p = {
        .tokens = tokens,
        .count = count,
        .pos = 0,
        .nodes = node_pool,
        .node_count = 0,
        .node_capacity = pool_size,
        .string_pool = string_pool,
        .string_pos = 0,
        .source = source,
        .has_error = false
    };
    
    // Create root program node
    uint16_t root = alloc_node(&p, NODE_PROGRAM);
    if (root == 0xFFFF) return 0xFFFF;
    
    // Parse all statements
    uint16_t body = parse_block(&p);
    
    if (p.has_error) {
        return 0xFFFF;
    }
    
    p.nodes[root].data.binary.left_idx = body;
    return root;
}

// Debug function
void debug_print_tokens(Token* tokens, uint16_t count, const char* source) {
    print_str("\n=== TOKENS ===\n");
    
    for (uint32_t i = 0; i < count && tokens[i].type != TOK_EOF; i++) {
        Token* t = &tokens[i];
        
        print_str("Line ");
        print_num(t->line);
        print_str(": ");
        
        // Print token type
        switch (t->type) {
            case TOK_VAR: print_str("VAR"); break;
            case TOK_ARRAY_4D: print_str("ARRAY_4D"); break;
            case TOK_GAP_COMPUTE: print_str("GAP_COMPUTE"); break;
            case TOK_PARAM: print_str("PARAM"); break;
            case TOK_MATRIX_START: print_str("MATRIX"); break;
            case TOK_TIMELINE_DEF: print_str("TIMELINE_DEF"); break;
            case TOK_TIMELINE_JUMP: print_str("TIMELINE_JUMP"); break;
            case TOK_ACTION_START: print_str("ACTION_START"); break;
            case TOK_CONNECTOR_FWD: print_str("CONN_FWD"); break;
            case TOK_CONNECTOR_BWD: print_str("CONN_BWD"); break;
            case TOK_TIMING_ONTO: print_str("ONTO"); break;
            case TOK_TIMING_INTO: print_str("INTO"); break;
            case TOK_TIMING_BOTH: print_str("BOTH"); break;
            case TOK_LT: print_str("LT"); break;
            case TOK_GT: print_str("GT"); break;
            case TOK_GREATER_THAN: print_str("GREATER_THAN"); break;
            case TOK_LESS_EQUAL: print_str("LESS_EQUAL"); break;
            case TOK_EQUAL: print_str("EQUAL"); break;
            case TOK_NOT_EQUAL: print_str("NOT_EQUAL"); break;
            case TOK_COND_CHK: print_str("COND_CHK"); break;
            case TOK_COND_ENS: print_str("COND_ENS"); break;
            case TOK_COND_VER: print_str("COND_VER"); break;
            case TOK_COND_IF: print_str("COND_IF"); break;
            case TOK_COND_TRY: print_str("COND_TRY"); break;
            case TOK_COND_GRD: print_str("COND_GRD"); break;
            case TOK_COND_UNL: print_str("COND_UNL"); break;
            case TOK_COND_WHL: print_str("COND_WHL"); break;
            case TOK_COND_UNT: print_str("COND_UNT"); break;
            case TOK_COND_OBS: print_str("COND_OBS"); break;
            case TOK_COND_DET: print_str("COND_DET"); break;
            case TOK_COND_REC: print_str("COND_REC"); break;
            case TOK_COND_FS: print_str("COND_FS"); break;
            case TOK_COND_EVAL: print_str("COND_EVAL"); break;
            case TOK_COND_MSR: print_str("COND_MSR"); break;
            case TOK_BNC: print_str("BNC"); break;
            case TOK_RECV: print_str("RECV"); break;
            case TOK_IDENTIFIER: print_str("IDENT"); break;
            case TOK_NUMBER: print_str("NUMBER"); break;
            case TOK_PIPE: print_str("PIPE"); break;
            case TOK_SLASH: print_str("SLASH"); break;
            case TOK_BACKSLASH: print_str("BACKSLASH"); break;
            case TOK_JUMP_MARKER: print_str("JUMP"); break;
            case TOK_MINUS: print_str("MINUS"); break;
            case TOK_BRACKET_OPEN: print_str("LBRACKET"); break;
            case TOK_BRACKET_CLOSE: print_str("RBRACKET"); break;
            case TOK_DOT: print_str("DOT"); break;
            case TOK_SEMICOLON: print_str("SEMICOLON"); break;
            case TOK_COLON: print_str("COLON"); break;
            case TOK_LBRACE: print_str("LBRACE"); break;
            case TOK_RBRACE: print_str("RBRACE"); break;
            case TOK_EOF: print_str("EOF"); break;
            default: 
                print_str("TOK(");
                print_num(t->type);
                print_str(")");
                break;
        }
        
        print_str(" \"");
        // Print actual token text
        for (uint32_t j = 0; j < t->len && j < 30; j++) {
            if (source[t->start + j] == '\n') {
                print_str("\\n");
            } else {
                char c[2] = {source[t->start + j], '\0'};
                print_str(c);
            }
        }
        if (t->len > 30) print_str("...");
        print_str("\"\n");
    }
    
    print_str("=== END TOKENS ===\n");
}