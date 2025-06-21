// Debug version of parser_core.c to find bus error
#include "blaze_internals.h"
#include <stdio.h>

// Parser state
typedef struct {
    Token* tokens;
    uint32_t count;
    uint32_t current;
    
    // AST node pool - pre-allocated
    ASTNode* nodes;
    uint32_t node_count;
    uint32_t node_capacity;
    
    // String pool for identifiers
    char* string_pool;
    uint32_t string_pos;
    
    // Source code reference
    const char* source;
    
    // Error state
    bool has_error;
    uint32_t error_pos;
} Parser;

// Test entry point
uint16_t parse_blaze_debug(Token* tokens, uint32_t count, ASTNode* node_pool, 
                          uint32_t pool_size, char* string_pool, const char* source) {
    printf("DEBUG: parse_blaze_debug called\n");
    printf("  tokens=%p, count=%u\n", tokens, count);
    printf("  node_pool=%p, pool_size=%u\n", node_pool, pool_size);
    printf("  string_pool=%p, source=%p\n", string_pool, source);
    
    if (!tokens) {
        printf("ERROR: tokens is NULL\n");
        return 0xFFFF;
    }
    
    if (!node_pool) {
        printf("ERROR: node_pool is NULL\n");
        return 0xFFFF;
    }
    
    if (!string_pool) {
        printf("ERROR: string_pool is NULL\n");
        return 0xFFFF;
    }
    
    if (!source) {
        printf("ERROR: source is NULL\n");
        return 0xFFFF;
    }
    
    printf("DEBUG: All pointers valid, creating parser\n");
    
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
    
    printf("DEBUG: Parser initialized\n");
    
    // Try to allocate first node
    if (parser.node_capacity == 0) {
        printf("ERROR: node_capacity is 0\n");
        return 0xFFFF;
    }
    
    printf("DEBUG: Allocating root node\n");
    uint16_t root = 0;
    
    // Simple allocation
    if (parser.node_count < parser.node_capacity) {
        root = parser.node_count++;
        parser.nodes[root].type = NODE_PROGRAM;
        printf("DEBUG: Root node allocated at index %u\n", root);
    } else {
        printf("ERROR: No space for root node\n");
        return 0xFFFF;
    }
    
    printf("DEBUG: parse_blaze_debug complete, returning %u\n", root);
    return root;
}