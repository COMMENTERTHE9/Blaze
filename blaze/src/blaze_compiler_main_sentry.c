// Example integration of Sentry into Blaze compiler main
// This shows how to add error tracking to your existing compiler

#include "blaze_internals.h"
#include "sentry_integration.h"

// ... (your existing includes and declarations) ...

int main(int argc, char** argv) {
    // Initialize Sentry first thing
    init_sentry();
    
    // Track command line arguments
    if (argc > 1) {
        char breadcrumb[256];
        snprintf(breadcrumb, sizeof(breadcrumb), "Compiling: %s", argv[1]);
        TRACK_COMPILATION_STEP("start", breadcrumb);
    }
    
    if (argc != 2) {
        REPORT_ERROR("ArgumentError", "Usage: blaze <source_file>");
        print_str("Usage: ");
        print_str(argv[0]);
        print_str(" <source_file>\n");
        cleanup_sentry();
        return 1;
    }
    
    // Read source file
    TRACK_COMPILATION_STEP("file_read", argv[1]);
    char source[MAX_SOURCE_SIZE];
    uint32_t source_len = read_file(argv[1], source, MAX_SOURCE_SIZE);
    if (source_len == 0) {
        REPORT_ERROR("FileError", "Failed to read source file");
        cleanup_sentry();
        return 1;
    }
    
    // Lexical analysis
    TRACK_COMPILATION_STEP("lexing", "Starting tokenization");
    Token tokens[MAX_TOKENS];
    uint32_t token_count = lex_blaze(source, source_len, tokens);
    if (token_count == 0) {
        REPORT_ERROR("LexError", "Failed to tokenize source");
        cleanup_sentry();
        return 1;
    }
    
    // Parsing
    TRACK_COMPILATION_STEP("parsing", "Building AST");
    ASTNode node_pool[MAX_AST_NODES];
    char string_pool[MAX_STRING_POOL];
    uint16_t root_idx = parse_blaze(tokens, token_count, node_pool, 
                                   MAX_AST_NODES, string_pool, source);
    
    if (root_idx == 0xFFFF) {
        REPORT_ERROR("ParseError", "Failed to parse source");
        cleanup_sentry();
        return 1;
    }
    
    // Example of catching AST errors
    if (node_pool[root_idx].type != NODE_PROGRAM) {
        report_ast_error("root", NODE_PROGRAM, node_pool[root_idx].type, "Expected PROGRAM node as root");
        cleanup_sentry();
        return 1;
    }
    
    // Code generation
    TRACK_COMPILATION_STEP("codegen", "Generating machine code");
    CodeBuffer code_buf = {0};
    code_buf.capacity = MAX_CODE_SIZE;
    
    // Try-catch style error handling for segfaults
    // (In real code, you'd use signal handlers)
    if (setjmp(error_jmp_buf) == 0) {
        generate_code(&code_buf, node_pool, root_idx, string_pool);
    } else {
        REPORT_SEGFAULT("Code generation crashed");
        cleanup_sentry();
        return 1;
    }
    
    // Generate executable
    TRACK_COMPILATION_STEP("linking", "Creating executable");
    const char* output_name = "output";
    Platform platform = get_default_platform();
    
    if (platform == PLATFORM_LINUX || platform == PLATFORM_MACOS) {
        generate_elf_executable(code_buf.data, code_buf.position, output_name);
    } else {
        generate_pe_executable(code_buf.data, code_buf.position, output_name);
    }
    
    // Success!
    sentry_capture_event(sentry_value_new_message_event(
        SENTRY_LEVEL_INFO,
        "blaze",
        "Compilation completed successfully"
    ));
    
    print_str("Compilation successful! Output: ");
    print_str(output_name);
    print_str("\n");
    
    cleanup_sentry();
    return 0;
}

// Example error handler for specific AST issues
void check_ast_node(ASTNode* nodes, uint16_t idx, NodeType expected_type) {
    if (nodes[idx].type != expected_type) {
        char context[256];
        snprintf(context, sizeof(context), "Node index: %d", idx);
        report_ast_error("check_ast_node", expected_type, nodes[idx].type, context);
        
        // You can also add breadcrumbs before the error
        sentry_value_t crumb = sentry_value_new_breadcrumb("error", "AST type mismatch detected");
        sentry_value_t data = sentry_value_new_object();
        sentry_value_set_by_key(data, "index", sentry_value_new_int32(idx));
        sentry_value_set_by_key(data, "expected", sentry_value_new_int32(expected_type));
        sentry_value_set_by_key(data, "actual", sentry_value_new_int32(nodes[idx].type));
        sentry_value_set_by_key(crumb, "data", data);
        sentry_add_breadcrumb(crumb);
    }
}