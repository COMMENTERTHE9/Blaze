// Debug parser - tests lexer and parser together
#include "../include/blaze_internals.h"
#include <stdio.h>

// External functions
extern uint32_t lex_blaze(const char* input, uint32_t len, Token* output);
extern uint16_t parse_blaze_v2(Token* tokens, uint32_t count, ASTNode* node_pool, 
                              uint32_t pool_size, char* string_pool, const char* source);

const char* token_names[] = {
    "EOF", "ACTION_START", "SLASH", "LT", "GT", "TIMING_ONTO", "TIMING_INTO", 
    "TIMING_BOTH", "FUNC_CLOSE", "CONNECTOR_FWD", "CONNECTOR_BWD", "BACKSLASH",
    "PIPE", "BRACKET_OPEN", "BRACKET_CLOSE", "JUMP_MARKER", "GLOBAL_ERROR",
    "VAR", "ARRAY_4D", "FUNC_CAN", "ERROR_CATCH", "GAP_COMPUTE",
    "GREATER_THAN", "LESS_EQUAL", "EQUAL", "NOT_EQUAL",
    "IDENTIFIER", "NUMBER", "STRING", "MINUS", "STAR", "COMMA",
    "DOT", "UNDERSCORE", "AT", "SEMICOLON", "PERCENT", "EQUALS",
    "LPAREN", "RPAREN", "LBRACE", "RBRACE", "COLON", "BANG",
    "PARAM", "MATRIX_START", "COND_ENS", "COND_VER", "COND_CHK",
    "COND_TRY", "COND_GRD", "COND_UNL", "COND_IF", "COND_WHL",
    "COND_UNT", "COND_OBS", "COND_DET", "COND_REC", "COND_FS",
    "COND_RTE", "COND_MON", "COND_EVAL", "COND_DEC", "COND_ASS", "COND_MSR",
    "TIMELINE_DEF", "TIMELINE_JUMP", "BNC", "RECV", "DO",
    "ACTION_CONTINUE", "ACTION_END", "BEFORE", "AFTER", "ONTO", "INTO", "BOTH",
    "FORWARD_CONN", "BACKWARD_CONN", "C_SPLIT",
    "PAST_ZONE", "PRESENT_ZONE", "FUTURE_ZONE", "UNKNOWN_ZONE"
};

const char* node_type_names[] = {
    "PROGRAM", "VAR_DEF", "FUNC_DEF", "ACTION_BLOCK", "TIMING_OP",
    "CONDITIONAL", "JUMP", "EXPRESSION", "BINARY_OP", "NUMBER",
    "IDENTIFIER", "ARRAY_4D", "ARRAY_4D_DEF", "ARRAY_4D_ACCESS",
    "GAP_ANALYSIS", "GAP_COMPUTE"
};

void print_token(Token* tok, const char* source) {
    printf("  [%3d] %-20s '", tok->type, 
           tok->type < sizeof(token_names)/sizeof(token_names[0]) 
           ? token_names[tok->type] : "UNKNOWN");
    
    for (int i = 0; i < tok->len && i < 40; i++) {
        char c = source[tok->start + i];
        if (c == '\n') printf("\\n");
        else if (c == '\t') printf("\\t");
        else printf("%c", c);
    }
    if (tok->len > 40) printf("...");
    printf("'\n");
}

void print_ast_node(ASTNode* nodes, uint16_t idx, char* string_pool, int depth) {
    if (idx == 0 || idx >= 4096) return;
    
    // Indent
    for (int i = 0; i < depth; i++) printf("  ");
    
    ASTNode* node = &nodes[idx];
    printf("[%d] %s", idx, node_type_names[node->type]);
    
    switch (node->type) {
        case NODE_NUMBER:
            printf(" = %ld\n", node->data.number);
            break;
            
        case NODE_IDENTIFIER:
            printf(" = %s\n", string_pool + node->data.ident.name_offset);
            break;
            
        case NODE_VAR_DEF:
            printf(" name=%s\n", string_pool + node->data.ident.name_offset);
            break;
            
        case NODE_FUNC_DEF:
            if (node->data.timing.expr_idx > 0) {
                printf(" name=%s\n", string_pool + node->data.timing.expr_idx);
            } else {
                printf("\n");
            }
            break;
            
        case NODE_BINARY_OP:
            printf(" op=%d\n", node->data.binary.op);
            print_ast_node(nodes, node->data.binary.left_idx, string_pool, depth + 1);
            print_ast_node(nodes, node->data.binary.right_idx, string_pool, depth + 1);
            break;
            
        case NODE_ACTION_BLOCK:
            printf("\n");
            print_ast_node(nodes, node->data.binary.left_idx, string_pool, depth + 1);
            break;
            
        case NODE_TIMING_OP:
            printf(" op=%d\n", node->data.timing.timing_op);
            print_ast_node(nodes, node->data.timing.expr_idx, string_pool, depth + 1);
            break;
            
        case NODE_CONDITIONAL:
            printf(" type=%d\n", node->data.binary.op);
            if (node->data.binary.left_idx) {
                for (int i = 0; i < depth + 1; i++) printf("  ");
                printf("Condition:\n");
                print_ast_node(nodes, node->data.binary.left_idx, string_pool, depth + 2);
            }
            if (node->data.binary.right_idx) {
                for (int i = 0; i < depth + 1; i++) printf("  ");
                printf("Body:\n");
                print_ast_node(nodes, node->data.binary.right_idx, string_pool, depth + 2);
            }
            break;
            
        case NODE_PROGRAM:
            printf("\n");
            // Walk the statement chain
            uint16_t stmt = node->data.binary.left_idx;
            while (stmt != 0 && stmt < 4096) {
                print_ast_node(nodes, stmt, string_pool, depth + 1);
                // Get next statement
                if (nodes[stmt].type == NODE_VAR_DEF || 
                    nodes[stmt].type == NODE_FUNC_DEF ||
                    nodes[stmt].type == NODE_ACTION_BLOCK ||
                    nodes[stmt].type == NODE_CONDITIONAL ||
                    nodes[stmt].type == NODE_JUMP) {
                    stmt = nodes[stmt].data.binary.right_idx;
                } else {
                    break;
                }
            }
            break;
            
        default:
            printf("\n");
            break;
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <source.blaze>\n", argv[0]);
        return 1;
    }
    
    // Read source file
    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        printf("Error: Cannot open file %s\n", argv[1]);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    printf("=== SOURCE ===\n%s\n", source);
    
    // Lex
    Token tokens[1024];
    uint32_t count = lex_blaze(source, size, tokens);
    
    printf("\n=== TOKENS (%d) ===\n", count);
    for (uint32_t i = 0; i < count; i++) {
        print_token(&tokens[i], source);
    }
    
    // Parse
    ASTNode nodes[4096];
    char string_pool[4096];
    
    uint16_t root = parse_blaze_v2(tokens, count, nodes, 4096, string_pool, source);
    
    if (root == 0) {
        printf("\n=== PARSE FAILED ===\n");
    } else {
        printf("\n=== AST ===\n");
        print_ast_node(nodes, root, string_pool, 0);
    }
    
    free(source);
    return 0;
}