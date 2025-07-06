#ifndef AST_NODE_H
#define AST_NODE_H

#include "blaze_internals.h"

// Node type definitions are in blaze_internals.h
// This header just provides forward declarations and common utilities

// Forward declarations
typedef struct ASTNode ASTNode;
typedef enum NodeType NodeType;

// Node pool management
#define MAX_NODES 4096

// Node access utilities
static inline bool is_node_valid(uint16_t node_idx) {
    return node_idx < MAX_NODES;
}

static inline bool is_node_type_valid(NodeType type) {
    return type >= 0 && type < NODE_TYPE_MAX;
}

#endif // AST_NODE_H 