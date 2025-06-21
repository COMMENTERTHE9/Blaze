// PARSER DEBUG VERSION - with trace output
#include "blaze_internals.h"
#include <stdio.h>

// Parser state (same as original)
typedef struct {
    Token* tokens;
    uint32_t count;
    uint32_t current;
    ASTNode* nodes;
    uint32_t node_count;
    uint32_t node_capacity;
    char* string_pool;
    uint32_t string_pos;
    const char* source;
    bool has_error;
    uint32_t error_pos;
    char error_msg[256];
} Parser;

// Forward declarations
static uint16_t parse_expression(Parser* p);
static uint16_t parse_statement(Parser* p);

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

// Allocate AST node
static uint16_t alloc_node(Parser* p, NodeType type) {
    printf("  alloc_node: type=%d, count=%d/%d\n", type, p->node_count, p->node_capacity);
    if (p->node_count >= p->node_capacity) {
        p->has_error = true;
        return 0;
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
    
    printf("  store_string: offset=%d, len=%d\n", offset, tok->len);
    
    // Copy token text to string pool
    for (uint16_t i = 0; i < tok->len; i++) {
        if (p->string_pos >= 4096) {
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
    printf("  extract_identifier: skip=%d, tok_len=%d\n", skip_prefix, tok->len);
    
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
    
    printf("    identifier range: %d to %d\n", start - tok->start, end - tok->start);
    
    // Copy identifier
    for (uint32_t i = start; i < end; i++) {
        if (p->string_pos >= 4096) {
            printf("    ERROR: string pool full\n");
            p->has_error = true;
            return 0;
        }
        p->string_pool[p->string_pos++] = p->source[i];
    }
    
    p->string_pool[p->string_pos++] = '\0';
    
    printf("    extracted: '%s'\n", p->string_pool + offset);
    
    return offset;
}

// Parse number literal
static uint16_t parse_number(Parser* p) {
    printf("  parse_number\n");
    Token* num_tok = advance(p);
    uint16_t node_idx = alloc_node(p, NODE_NUMBER);
    
    if (node_idx == 0) return 0;
    
    // Convert string to number
    int64_t value = 0;
    for (uint16_t i = 0; i < num_tok->len; i++) {
        char c = p->source[num_tok->start + i];
        if (c >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
        }
    }
    
    printf("    value: %ld\n", value);
    p->nodes[node_idx].data.number = value;
    return node_idx;
}

// Parse variable definition
static uint16_t parse_var_def(Parser* p) {
    printf("parse_var_def: current=%d\n", p->current);
    
    Token* var_tok = advance(p); // consume TOK_VAR
    printf("  consumed VAR token, len=%d\n", var_tok->len);
    
    uint16_t var_node = alloc_node(p, NODE_VAR_DEF);
    if (var_node == 0) {
        printf("  ERROR: failed to alloc node\n");
        return 0;
    }
    
    // Extract variable name from token
    uint32_t name_offset = extract_identifier(p, var_tok, 6); // Skip "var.v-"
    if (p->has_error) {
        printf("  ERROR: extract_identifier failed\n");
        return 0;
    }
    
    p->nodes[var_node].data.ident.name_offset = name_offset;
    
    // Check if there's an initialization value
    printf("  checking for init, current token type=%d\n", 
           p->current < p->count ? p->tokens[p->current].type : -1);
           
    if (check(p, TOK_BRACKET_OPEN)) {
        printf("  found bracket open\n");
        advance(p); // consume [
        
        // Parse initialization expression
        uint16_t init_expr = parse_expression(p);
        if (init_expr == 0) {
            printf("  ERROR: parse_expression failed\n");
            return 0;
        }
        
        printf("  init expression node: %d\n", init_expr);
        
        // Store init expression in high bits of name_len (hacky but works)
        p->nodes[var_node].data.ident.name_len |= (init_expr << 16);
        
        if (!match(p, TOK_BRACKET_CLOSE)) {
            printf("  ERROR: expected ]\n");
            p->has_error = true;
            return 0;
        }
    }
    
    printf("  var_def success, node=%d\n", var_node);
    return var_node;
}

// Parse expression
static uint16_t parse_expression(Parser* p) {
    printf("  parse_expression: current=%d, type=%d\n", 
           p->current, p->current < p->count ? p->tokens[p->current].type : -1);
    
    // Numbers
    if (check(p, TOK_NUMBER)) {
        return parse_number(p);
    }
    
    printf("  ERROR: unhandled expression type\n");
    p->has_error = true;
    return 0;
}

// Parse statement
static uint16_t parse_statement(Parser* p) {
    printf("parse_statement: current=%d, type=%d\n", 
           p->current, p->current < p->count ? p->tokens[p->current].type : -1);
    
    // Variable definition
    if (check(p, TOK_VAR)) {
        return parse_var_def(p);
    }
    
    printf("ERROR: unhandled statement type\n");
    p->has_error = true;
    return 0;
}

// Main parse function
uint16_t parse_blaze_debug(Token* tokens, uint32_t count, ASTNode* node_pool, 
                          uint32_t pool_size, char* string_pool, const char* source) {
    printf("=== PARSER DEBUG START ===\n");
    printf("Token count: %d, pool size: %d\n", count, pool_size);
    
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
    if (program_node == 0) {
        printf("ERROR: failed to create program node\n");
        return 0;
    }
    
    printf("Created program node: %d\n", program_node);
    
    // Parse first statement
    uint16_t stmt = parse_statement(&parser);
    
    if (parser.has_error) {
        printf("ERROR: parse failed at position %d\n", parser.error_pos);
        return 0;
    }
    
    printf("First statement: %d\n", stmt);
    parser.nodes[program_node].data.binary.left_idx = stmt;
    
    printf("=== PARSER DEBUG END ===\n");
    return program_node;
}