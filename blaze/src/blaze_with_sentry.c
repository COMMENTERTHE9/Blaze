// Example integration of simple error tracking into Blaze compiler
#include "blaze_internals.h"
#include "simple_sentry.h"

// Your existing external declarations...
extern uint32_t lex_blaze(const char* input, uint32_t len, Token* output);
extern uint16_t parse_blaze(Token* tokens, uint32_t count, ASTNode* node_pool, 
                           uint32_t pool_size, char* string_pool, const char* source);
// ... other declarations ...

// Function to validate AST nodes with error tracking
void validate_ast_node(ASTNode* nodes, uint16_t idx, const char* context) {
    if (idx >= MAX_AST_NODES) {
        SENTRY_ERROR("AST_BOUNDS", "Node index out of bounds");
        return;
    }
    
    ASTNode* node = &nodes[idx];
    
    // Track the problematic 243 type
    if (node->type == 243) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Found suspicious type 243 at index %d in %s", idx, context);
        SENTRY_ERROR("AST_TYPE_243", msg);
        
        // Log more details
        SENTRY_BREADCRUMB("ast_debug", "Investigating type 243 node");
        
        // Check what this node should be
        if (node->data.number == 2) {
            report_ast_error(context, NODE_FUNC_DEF, 243);
        }
    }
}

int main(int argc, char** argv) {
    // Initialize error tracking
    SENTRY_INIT();
    SENTRY_BREADCRUMB("startup", "Blaze compiler starting");
    
    if (argc != 2) {
        SENTRY_ERROR("ARGS", "Invalid command line arguments");
        print_str("Usage: blaze <source_file>\n");
        SENTRY_CLEANUP();
        return 1;
    }
    
    // Log compilation target
    char breadcrumb[256];
    snprintf(breadcrumb, sizeof(breadcrumb), "Compiling: %s", argv[1]);
    SENTRY_BREADCRUMB("compile", breadcrumb);
    
    // Read source file
    char source[MAX_SOURCE_SIZE];
    uint32_t source_len = read_file(argv[1], source, MAX_SOURCE_SIZE);
    if (source_len == 0) {
        SENTRY_ERROR("FILE_READ", "Failed to read source file");
        SENTRY_CLEANUP();
        return 1;
    }
    
    // Lexing
    SENTRY_BREADCRUMB("lexer", "Starting tokenization");
    Token tokens[MAX_TOKENS];
    uint32_t token_count = lex_blaze(source, source_len, tokens);
    if (token_count == 0) {
        SENTRY_ERROR("LEX", "Tokenization failed");
        SENTRY_CLEANUP();
        return 1;
    }
    snprintf(breadcrumb, sizeof(breadcrumb), "Tokenized: %d tokens", token_count);
    SENTRY_BREADCRUMB("lexer", breadcrumb);
    
    // Parsing
    SENTRY_BREADCRUMB("parser", "Building AST");
    ASTNode node_pool[MAX_AST_NODES];
    char string_pool[MAX_STRING_POOL];
    uint16_t root_idx = parse_blaze(tokens, token_count, node_pool, 
                                   MAX_AST_NODES, string_pool, source);
    
    if (root_idx == 0xFFFF) {
        SENTRY_ERROR("PARSE", "Failed to parse source");
        SENTRY_CLEANUP();
        return 1;
    }
    
    // Validate AST structure
    SENTRY_BREADCRUMB("ast_validation", "Checking AST integrity");
    validate_ast_node(node_pool, root_idx, "root");
    
    // Walk through AST looking for problematic nodes
    for (uint16_t i = 0; i < MAX_AST_NODES && node_pool[i].type != 0; i++) {
        if (node_pool[i].type == 243) {
            char ctx[64];
            snprintf(ctx, sizeof(ctx), "node_%d", i);
            validate_ast_node(node_pool, i, ctx);
        }
    }
    
    // Code generation
    SENTRY_BREADCRUMB("codegen", "Generating machine code");
    CodeBuffer code_buf = {0};
    code_buf.capacity = MAX_CODE_SIZE;
    
    // Your code generation here...
    // generate_code(&code_buf, node_pool, root_idx, string_pool);
    
    SENTRY_BREADCRUMB("complete", "Compilation finished");
    print_str("Compilation successful!\n");
    
    SENTRY_CLEANUP();
    return 0;
}