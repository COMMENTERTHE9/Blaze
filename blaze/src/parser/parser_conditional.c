// Parser for conditional abbreviations in Blaze
// Handles f.xxx/param\>|body and fucn.xxx/param\>|body syntax

#include "blaze_internals.h"

// Forward declarations from parser internals
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

// External parser utilities
extern bool at_end(Parser* p);
extern Token* peek(Parser* p);
extern Token* advance(Parser* p);
extern bool check(Parser* p, TokenType type);
extern bool match(Parser* p, TokenType type);
extern uint16_t alloc_node(Parser* p, NodeType type);
extern uint32_t store_string(Parser* p, const char* str, uint32_t len);
extern uint16_t parse_expression(Parser* p);
extern uint16_t parse_statement(Parser* p);

// Parse conditional: f.xxx/param\>|body or fucn.xxx/param\>|body
uint16_t parse_conditional_statement(Parser* p) {
    Token* cond_tok = peek(p);
    
    // Check if this is a conditional token
    if (cond_tok->type < TOK_COND_ENS || cond_tok->type > TOK_COND_MSR) {
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

// Check if a token is a conditional
bool is_conditional_token(TokenType type) {
    return type >= TOK_COND_ENS && type <= TOK_COND_MSR;
}