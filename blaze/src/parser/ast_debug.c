// AST DEBUG PRINTER - Helps visualize the parse tree
// Only used for debugging, not in final compiler

#include "blaze_internals.h"

// Both print_str and print_num are already defined in blaze_internals.h

// Node type names for debugging
static const char* node_type_names[] = {
    "PROGRAM",
    "VAR_DEF", 
    "FUNC_DEF",
    "ACTION_BLOCK",
    "TIMING_OP",
    "CONDITIONAL",
    "JUMP",
    "EXPRESSION",
    "BINARY_OP",
    "NUMBER",
    "IDENTIFIER",
    "ARRAY_4D",
    "GAP_ANALYSIS"
};

// Token type to string
static const char* token_type_str(TokenType type) {
    switch (type) {
        case TOK_LT: return "<";
        case TOK_GT: return ">";
        case TOK_TIMING_ONTO: return "<<";
        case TOK_TIMING_INTO: return ">>";
        case TOK_TIMING_BOTH: return "<>";
        case TOK_CONNECTOR_FWD: return "\\>|";
        case TOK_CONNECTOR_BWD: return "\\<|";
        case TOK_MINUS: return "-";
        case TOK_SLASH: return "/";
        case TOK_GREATER_THAN: return "*>";
        case TOK_LESS_EQUAL: return "*_<";
        case TOK_EQUAL: return "*=";
        case TOK_NOT_EQUAL: return "*!=";
        default: return "?";
    }
}

// Print helper
static void print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        print_str("  ");
    }
}

static void print_node_name(const char* name) {
    print_str(name);
}

// Recursive AST printer
void print_ast_node(ASTNode* nodes, uint16_t node_idx, char* string_pool, int depth) {
    if (node_idx == 0 || node_idx >= 4096) return;
    
    ASTNode* node = &nodes[node_idx];
    
    print_indent(depth);
    print_node_name(node_type_names[node->type]);
    
    switch (node->type) {
        case NODE_NUMBER:
            print_str("(");
            print_num(node->data.number);
            print_str(")");
            break;
            
        case NODE_IDENTIFIER:
            print_str("(");
            print_str(&string_pool[node->data.ident.name_offset]);
            print_str(")");
            break;
            
        case NODE_BINARY_OP:
            print_str("(");
            print_str(token_type_str(node->data.binary.op));
            print_str(")\n");
            print_ast_node(nodes, node->data.binary.left_idx, string_pool, depth + 1);
            print_ast_node(nodes, node->data.binary.right_idx, string_pool, depth + 1);
            return;
            
        case NODE_TIMING_OP:
            print_str("(");
            print_str(token_type_str(node->data.timing.timing_op));
            print_str(" offset=");
            print_num(node->data.timing.temporal_offset);
            print_str(")\n");
            print_ast_node(nodes, node->data.timing.expr_idx, string_pool, depth + 1);
            return;
            
        case NODE_VAR_DEF:
            print_str("(");
            print_str(&string_pool[node->data.ident.name_offset]);
            print_str(")");
            // Check for initializer (stored in upper bits of name_len)
            uint16_t init_idx = node->data.ident.name_len >> 16;
            if (init_idx > 0) {
                print_str("\n");
                print_indent(depth + 1);
                print_str("INIT:\n");
                print_ast_node(nodes, init_idx, string_pool, depth + 2);
                return;
            }
            break;
            
        case NODE_FUNC_DEF:
            print_str("(name_offset=");
            print_num(node->data.timing.expr_idx);
            if (node->data.timing.temporal_offset) {
                print_str(" has_closer");
            }
            print_str(")");
            break;
            
        case NODE_ACTION_BLOCK:
            print_str("\n");
            // Actions are chained via binary.left_idx
            uint16_t action = node->data.binary.left_idx;
            while (action != 0 && action < 4096) {
                print_ast_node(nodes, action, string_pool, depth + 1);
                // Get next action from right_idx chain
                if (nodes[action].type == NODE_BINARY_OP || 
                    nodes[action].type == NODE_EXPRESSION) {
                    action = nodes[action].data.binary.right_idx;
                } else {
                    break;
                }
            }
            return;
            
        case NODE_CONDITIONAL:
            print_str("(op=");
            print_str(token_type_str(node->data.binary.op));
            print_str(")\n");
            print_indent(depth + 1);
            print_str("PARAM:\n");
            print_ast_node(nodes, node->data.binary.left_idx, string_pool, depth + 2);
            return;
            
        case NODE_JUMP:
            print_str("(target_offset=");
            print_num(node->data.ident.name_offset);
            print_str(")");
            break;
            
        case NODE_PROGRAM:
            print_str("\n");
            // Statements are chained
            uint16_t stmt = node->data.binary.left_idx;
            while (stmt != 0 && stmt < 4096) {
                print_ast_node(nodes, stmt, string_pool, depth + 1);
                print_str("\n");
                // Try to get next statement
                if (stmt < 4096 && nodes[stmt].data.binary.right_idx != 0) {
                    stmt = nodes[stmt].data.binary.right_idx;
                } else {
                    break;
                }
            }
            return;
    }
    
    print_str("\n");
}

// Main debug print function
void debug_print_ast(ASTNode* nodes, uint16_t root, char* string_pool) {
    print_str("\n=== AST STRUCTURE ===\n");
    print_ast_node(nodes, root, string_pool, 0);
    print_str("\n=== END AST ===\n");
}