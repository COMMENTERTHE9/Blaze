// BLAZE COMPILER MAIN - Integrates all components
// Compiles Blaze source to x86-64 machine code

#include "blaze_internals.h"
#include "simple_sentry.h"

// External function declarations from all modules
extern uint32_t lex_blaze(const char* input, uint32_t len, Token* output);
extern uint16_t parse_blaze(Token* tokens, uint32_t count, ASTNode* node_pool, 
                           uint32_t pool_size, char* string_pool, const char* source);
extern void generate_temporal_function(CodeBuffer* code, ASTNode* nodes, uint16_t root_idx,
                                     uint16_t node_count, char* string_pool,
                                     ExecutionStep* execution_plan, uint32_t plan_size);
extern void generate_array4d_create(CodeBuffer* buf, ASTNode* nodes, uint16_t node_idx, 
                                   SymbolTable* symbols, char* string_pool);
extern void generate_array4d_access(CodeBuffer* buf, ASTNode* nodes, uint16_t node_idx,
                                   SymbolTable* symbols, char* string_pool, bool is_lvalue);
extern void generate_elf_executable(uint8_t* machine_code, uint32_t code_size, 
                                   const char* output_filename);
extern void generate_pe_executable(uint8_t* machine_code, uint32_t code_size,
                                  const char* output_filename);
extern Platform get_default_platform();
extern void emit_platform_exit(CodeBuffer* buf, Platform platform, int exit_code);
extern void emit_sub_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void gen_timeline_operation(uint8_t* output, uint32_t* offset, ASTNode* node, 
                                  char* string_pool, SymbolTable* symbols);
extern void gen_output_method(uint8_t* output, uint32_t* offset, ASTNode* node,
                             char* string_pool, SymbolTable* symbols);
extern void gen_inline_asm(uint8_t* output, uint32_t* offset, ASTNode* node,
                          char* string_pool, SymbolTable* symbols);

// Simple file reading (no stdlib)
static uint32_t read_file(const char* filename, char* buffer, uint32_t max_size) {
    // Use syscalls to read file
    int fd;
    __asm__ volatile (
        "movq $2, %%rax\n"       // sys_open
        "movq %1, %%rdi\n"       // filename
        "movq $0, %%rsi\n"       // O_RDONLY
        "movq $0, %%rdx\n"       // mode
        "syscall\n"
        "movl %%eax, %0\n"
        : "=r"(fd)
        : "r"(filename)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
    );
    
    if (fd < 0) {
        print_str("Error: Could not open file ");
        print_str(filename);
        print_str("\n");
        return 0;
    }
    
    // Read file
    int32_t bytes_read;
    __asm__ volatile (
        "movq $0, %%rax\n"       // sys_read
        "movl %1, %%edi\n"       // fd
        "movq %2, %%rsi\n"       // buffer
        "movq %3, %%rdx\n"       // count
        "syscall\n"
        "movl %%eax, %0\n"
        : "=r"(bytes_read)
        : "r"(fd), "r"(buffer), "r"((uint64_t)max_size)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
    );
    
    // Close file
    __asm__ volatile (
        "movq $3, %%rax\n"       // sys_close
        "movl %0, %%edi\n"       // fd
        "syscall\n"
        :
        : "r"(fd)
        : "rax", "rdi", "rcx", "r11"
    );
    
    if (bytes_read < 0 || bytes_read > max_size) {
        return 0;
    }
    
    // Null-terminate the buffer
    buffer[bytes_read] = '\0';
    
    return bytes_read;
}

// Forward declarations for codegen
extern void generate_statement(CodeBuffer* buf, ASTNode* nodes, uint16_t stmt_idx,
                              SymbolTable* symbols, char* string_pool);
extern void generate_output(CodeBuffer* buf, ASTNode* nodes, uint16_t node_idx,
                           SymbolTable* symbols, char* string_pool);
extern void generate_expression(CodeBuffer* buf, ASTNode* nodes, uint16_t expr_idx,
                               SymbolTable* symbols, char* string_pool);

// Helper to print compiled size
static void print_size(uint32_t size) {
    char size_str[16];
    int idx = 0;
    uint32_t n = size;
    do {
        size_str[idx++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    while (idx > 0) {
        write(1, &size_str[--idx], 1);
    }
    write(1, " bytes of machine code\n", 23);
}

// Global static buffers to avoid stack issues
static volatile char source_buffer[32768] = {0};
static volatile Token tokens[MAX_TOKENS] = {0};
static volatile ASTNode nodes[4096] = {0};
static volatile char string_pool[4096] = {0};
static volatile uint8_t code_buffer[MAX_CODE_SIZE] = {0};
static volatile ExecutionStep execution_plan[1024] = {0};
static volatile SymbolTable symbols = {0};

// Main compiler entry point
int main(int argc, char** argv) {
    // Initialize error tracking
    SENTRY_INIT();
    SENTRY_BREADCRUMB("startup", "Blaze compiler starting");
    
    // Initialize first token to avoid issues
    tokens[0].type = TOK_EOF;
    tokens[0].start = 0;
    tokens[0].len = 0;
    tokens[0].line = 0;
    
    // Initialize first node
    nodes[0].type = 0;
    
    // Initialize all nodes to ensure clean state
    for (int i = 0; i < 4096; i++) {
        nodes[i].type = 0;
        // Clear the data union
        uint8_t* data = (uint8_t*)&nodes[i].data;
        for (int j = 0; j < sizeof(nodes[i].data); j++) {
            data[j] = 0;
        }
    }
    
    // Initialize first execution plan entry
    execution_plan[0].node_idx = 0;
    execution_plan[0].creates_past_value = false;
    execution_plan[0].requires_future_value = false;
    execution_plan[0].temporal_order = 0;
    execution_plan[0].dep_count = 0;
    
    // Parse command line arguments
    if (argc < 3 || argc > 4) {
        const char* usage = "Usage: blaze <input.blaze> <output> [--windows]\n";
        write(1, usage, str_len(usage));
        return 1;
    }
    
    // Check for platform override
    Platform target_platform = get_default_platform();
    if (argc >= 4) {
        // Check third or fourth argument for platform flag
        for (int i = 3; i < argc; i++) {
            const char* flag = argv[i];
            if (flag[0] == '-' && flag[1] == '-') {
                if (flag[2] == 'w' && flag[3] == 'i' && flag[4] == 'n' && 
                    flag[5] == 'd' && flag[6] == 'o' && flag[7] == 'w' && 
                    flag[8] == 's' && flag[9] == '\0') {
                    target_platform = PLATFORM_WINDOWS;
                    print_str("[MAIN] Targeting Windows platform\n");
                } else if (flag[2] == 'p' && flag[3] == 'l' && flag[4] == 'a' &&
                          flag[5] == 't' && flag[6] == 'f' && flag[7] == 'o' &&
                          flag[8] == 'r' && flag[9] == 'm' && flag[10] == '\0' &&
                          i + 1 < argc) {
                    // --platform <name>
                    const char* platform = argv[++i];
                    if (platform[0] == 'w' && platform[1] == 'i' && platform[2] == 'n' &&
                        platform[3] == 'd' && platform[4] == 'o' && platform[5] == 'w' &&
                        platform[6] == 's' && platform[7] == '\0') {
                        target_platform = PLATFORM_WINDOWS;
                        print_str("[MAIN] Targeting Windows platform\n");
                    }
                }
            }
        }
    }
    
    // Read source file
    
    uint32_t source_len = read_file(argv[1], source_buffer, 32767);
    if (source_len == 0) {
        return 1;
    }
    
    // Tokenize
    uint32_t token_count = lex_blaze(source_buffer, source_len, tokens);
    if (token_count == 0) {
        print_str("Error: No tokens generated\n");
        return 1;
    }
    
    // Parse
    print_str("[MAIN] Starting parse with ");
    print_num(token_count);
    print_str(" tokens\n");
    print_str("[MAIN] Calling parse_blaze...\n");
    SENTRY_BREADCRUMB("parse", "Starting AST parsing");
    uint16_t root_idx = parse_blaze(tokens, token_count, nodes, 4096, string_pool, source_buffer);
    print_str("[MAIN] Parse returned root_idx=");
    print_num(root_idx);
    print_str("\n");
    if (root_idx == 0) {
        print_str("Error: Parse failed\n");
        SENTRY_ERROR("PARSE_FAIL", "Parser returned 0");
        SENTRY_CLEANUP();
        return 1;
    }
    
    // Check for type 243 error
    SENTRY_BREADCRUMB("ast_check", "Checking for type 243 errors");
    for (uint16_t i = 0; i < 100 && nodes[i].type != 0; i++) {
        if (nodes[i].type == 243) {
            print_str("[ERROR] Found type 243 at node ");
            print_num(i);
            print_str("\n");
            char msg[256];
            uint64_t* ptr = (uint64_t*)&nodes[i];
            print_str("Node data: ");
            for (int j = 0; j < 4; j++) {
                print_num(ptr[j]);
                print_str(" ");
            }
            print_str("\n");
            SENTRY_ERROR("AST_TYPE_243", "Found corrupt AST node with type 243");
        }
    }
    print_str("[MAIN] Root node type=");
    print_num(nodes[root_idx].type);
    print_str("\n");
    print_str("[MAIN] nodes[1].type=");
    print_num(nodes[1].type);
    print_str(" left_idx=");
    print_num(nodes[1].data.binary.left_idx);
    print_str(" right_idx=");
    print_num(nodes[1].data.binary.right_idx);
    print_str("\n");
    print_str("[MAIN] nodes[2].type=");
    print_num(nodes[2].type);
    print_str(" data.binary.left_idx=");
    print_num(nodes[2].data.binary.left_idx);
    print_str(" right_idx=");
    print_num(nodes[2].data.binary.right_idx);
    print_str("\n");
    
    // Symbol table
    print_str("[MAIN] Initializing symbol table\n");
    symbol_table_init(&symbols, string_pool);
    
    print_str("[MAIN] Building symbol table\n");
    print_str("[MAIN] Passing nodes addr=");
    print_num((uint64_t)nodes);
    print_str(" to symbol builder\n");
    if (!build_symbol_table(&symbols, nodes, root_idx, 4096, string_pool)) {
        print_str("Error: Symbol table build failed\n");
        return 1;
    }
    print_str("[MAIN] Symbol table built successfully\n");
    
    // Time travel analysis
    uint32_t plan_size = 0;
    if (!resolve_time_travel(nodes, root_idx, 4096, string_pool, execution_plan)) {
        // No time travel needed - continue with normal compilation
        plan_size = 0;
    } else {
        // Count execution steps
        while (plan_size < 1024 && execution_plan[plan_size].node_idx != 0) {
            plan_size++;
        }
    }
    
    // Code generation
    // Clear code buffer to ensure no garbage
    for (uint32_t i = 0; i < 1024; i++) {
        code_buffer[i] = 0;
    }
    
    CodeBuffer code_buf;
    code_buf.code = code_buffer;
    code_buf.position = 0;
    code_buf.capacity = MAX_CODE_SIZE;
    code_buf.has_error = false;
    code_buf.temporal_count = 0;
    code_buf.entry_point = 0;
    code_buf.main_call_offset_pos = 0;
    code_buf.bss_offsets_need_patch = false;
    code_buf.target_platform = target_platform;
    
    // Initialize runtime first
    extern void generate_runtime_init_minimal(CodeBuffer* buf);
    generate_runtime_init_minimal(&code_buf);
    
    // Initialize variable storage before generating statements
    extern void generate_var_storage_init(CodeBuffer* buf);
    generate_var_storage_init(&code_buf);
    
    // Generate code
    print_str("[MAIN] Starting code generation for root_idx=");
    print_num(root_idx);
    print_str("\n");
    generate_statement(&code_buf, nodes, root_idx, &symbols, string_pool);
    
    // Check for buffer overflow errors
    if (code_buf.has_error) {
        print_str("[MAIN] ERROR: Code generation failed - buffer overflow!\n");
        SENTRY_ERROR("Code generation buffer overflow", "Buffer capacity exceeded");
        SENTRY_CLEANUP();
        return 1;
    }
    
    print_str("[MAIN] Code generation completed\n");
    
    // Clean up variable storage before exit
    // extern void generate_var_storage_cleanup(CodeBuffer* buf);
    // generate_var_storage_cleanup(&code_buf);
    
    // Exit cleanly
    emit_platform_exit(&code_buf, code_buf.target_platform, 0);
    
    // Final error check before writing
    if (code_buf.has_error) {
        print_str("[MAIN] ERROR: Late buffer overflow detected!\n");
        SENTRY_ERROR("Late buffer overflow", "Buffer overflow during cleanup");
        SENTRY_CLEANUP();
        return 1;
    }
    
    // Write executable based on platform
    if (code_buf.target_platform == PLATFORM_WINDOWS) {
        generate_pe_executable(code_buffer, code_buf.position, argv[2]);
    } else {
        generate_elf_executable(code_buffer, code_buf.position, argv[2]);
    }
    
    SENTRY_BREADCRUMB("complete", "Compilation successful");
    SENTRY_CLEANUP();
    return 0;
}