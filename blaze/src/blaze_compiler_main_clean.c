// BLAZE COMPILER MAIN - Integrates all components
// Compiles Blaze source to x86-64 machine code

#include "blaze_internals.h"

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
extern void gen_output_method(CodeBuffer* buf, ASTNode* node,
                             char* string_pool, SymbolTable* symbols);
extern void gen_inline_asm(uint8_t* output, uint32_t* offset, ASTNode* node,
                          char* string_pool, SymbolTable* symbols);
extern void generate_runtime_init_minimal(CodeBuffer* buf);

// Simple file reading (no stdlib)
static uint32_t read_file(const char* filename, char* buffer, uint32_t max_size) {
    print_str("[READ_FILE] Called with filename=");
    print_str(filename);
    print_str(" buffer addr=");
    print_num((uint64_t)buffer);
    print_str(" max_size=");
    print_num(max_size);
    print_str("\n");
    
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
    
    print_str("[READ_FILE] Read syscall returned bytes_read=");
    print_num(bytes_read);
    print_str("\n");
    
    // Close file
    __asm__ volatile (
        "movq $3, %%rax\n"       // sys_close
        "movl %0, %%edi\n"       // fd
        "syscall\n"
        :
        : "r"(fd)
        : "rax", "rdi", "rcx", "r11", "memory"
    );
    
    print_str("[READ_FILE] Returning ");
    print_num((bytes_read < 0) ? 0 : bytes_read);
    print_str(" bytes\n");
    
    return (bytes_read < 0) ? 0 : bytes_read;
}

// Utility function for string comparison
static bool str_equals(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2) return false;
        s1++;
        s2++;
    }
    return *s1 == *s2;
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
static char source_buffer[32768] = {0};
static Token tokens[MAX_TOKENS] = {0};
static ASTNode nodes[4096] = {0};
static char string_pool[4096] = {0};
static uint8_t code_buffer[MAX_CODE_SIZE] = {0};
static ExecutionStep execution_plan[1024] = {0};
static SymbolTable symbols = {0};

// Main compiler entry point
int main(int argc, char** argv) {
    
    // Clear buffers to avoid garbage
    for (int i = 0; i < 32768; i++) source_buffer[i] = 0;
    for (int i = 0; i < MAX_TOKENS; i++) {
        tokens[i].type = TOK_EOF;
        tokens[i].start = 0;
        tokens[i].len = 0;
        tokens[i].line = 0;
    }
    
    // Clear critical buffers
    nodes[0].type = 0;
    for (int j = 0; j < sizeof(nodes[0].data); j++) {
        ((uint8_t*)&nodes[0].data)[j] = 0;
    }
    
    for (int i = 0; i < 4096; i++) string_pool[i] = 0;
    for (int i = 0; i < 1024; i++) code_buffer[i] = 0;
    
    for (int i = 0; i < 1024; i++) {
        execution_plan[i].node_idx = 0;
        execution_plan[i].creates_past_value = false;
        execution_plan[i].requires_future_value = false;
        execution_plan[i].temporal_order = 0;
        execution_plan[i].dep_count = 0;
    }
    
    // Parse command line arguments
    if (argc < 3) {
        const char* usage = "Usage: blaze <input.blaze> <output> [--platform linux|windows|macos]\n";
        write(1, usage, str_len(usage));
        return 1;
    }
    
    // Detect target platform
    Platform target_platform = PLATFORM_LINUX;  // Default
    
    // Check for platform flags
    for (int i = 3; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == '-') {
            // Check for --windows shorthand
            if (str_equals(argv[i], "--windows")) {
                target_platform = PLATFORM_WINDOWS;
                print_str("[MAIN] Target platform: Windows\n");
                break;
            }
            // Check for --platform <name>
            else if (str_equals(argv[i], "--platform") && i + 1 < argc) {
                i++; // Move to platform name
                if (str_equals(argv[i], "windows")) {
                    target_platform = PLATFORM_WINDOWS;
                    print_str("[MAIN] Target platform: Windows\n");
                } else if (str_equals(argv[i], "macos")) {
                    target_platform = PLATFORM_MACOS;
                    print_str("[MAIN] Target platform: macOS\n");
                } else if (str_equals(argv[i], "linux")) {
                    target_platform = PLATFORM_LINUX;
                    print_str("[MAIN] Target platform: Linux\n");
                } else {
                    print_str("Error: Unknown platform. Use linux, windows, or macos\n");
                    return 1;
                }
                break;
            }
        }
    }
    
    // Read source file
    uint32_t source_len = read_file(argv[1], source_buffer, 32767);
    print_str("[MAIN] After read_file, source_len=");
    print_num(source_len);
    print_str(" at addr=");
    print_num((uint64_t)&source_len);
    print_str("\n");
    
    if (source_len == 0) {
        return 1;
    }
    
    print_str("[MAIN] Before lex_blaze, source_len=");
    print_num(source_len);
    print_str("\n");
    
    // Tokenize
    print_str("[MAIN] Calling lex_blaze with source_len=");
    print_num(source_len);
    print_str("\n");
    
    // Debug: Set RSI explicitly before the call
    print_str("[MAIN] Setting up call: buffer=");
    print_num((uint64_t)source_buffer);
    print_str(" len=");
    print_num(source_len);
    print_str(" tokens=");
    print_num((uint64_t)tokens);
    print_str("\n");
    
    uint32_t token_count = lex_blaze(source_buffer, source_len, tokens);
    print_str("[MAIN] lex_blaze returned token_count=");
    print_num(token_count);
    print_str("\n");
    if (token_count == 0) {
        print_str("Error: No tokens generated\n");
        return 1;
    }
    
    // Parse
    print_str("[MAIN] Calling parse_blaze with token_count=");
    print_num(token_count);
    print_str(" nodes=");
    print_num((uint64_t)nodes);
    print_str("\n");
    uint16_t root_idx = parse_blaze(tokens, token_count, nodes, 4096, string_pool, source_buffer);
    if (root_idx == 0) {
        print_str("Error: Parse failed\n");
        return 1;
    }
    
    // Symbol table
    static SymbolTable symbols;
    symbol_table_init(&symbols, string_pool);
    
    if (!build_symbol_table(&symbols, nodes, root_idx, 4096, string_pool)) {
        print_str("Error: Symbol table build failed\n");
        return 1;
    }
    
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
    CodeBuffer code_buf = {
        .code = code_buffer,
        .position = 0,
        .capacity = MAX_CODE_SIZE,
        .has_error = false,
        .temporal_count = 0,
        .target_platform = target_platform
    };
    
    // Initialize runtime with minimal setup
    generate_runtime_init_minimal(&code_buf);
    
    // Initialize variable storage
    extern void generate_var_storage_init(CodeBuffer* buf);
    generate_var_storage_init(&code_buf);
    
    // Generate code
    generate_statement(&code_buf, nodes, root_idx, &symbols, string_pool);
    
    // Check for buffer overflow errors
    if (code_buf.has_error) {
        print_str("[ERROR] Code generation failed - buffer overflow!\n");
        return 1;
    }
    
    // Don't clean up variable storage at top level - we're not in a function
    // extern void generate_var_storage_cleanup(CodeBuffer* buf);
    // generate_var_storage_cleanup(&code_buf);
    
    print_str("[MAIN] About to emit platform exit\n");
    
    // Exit cleanly
    emit_platform_exit(&code_buf, target_platform, 0);
    
    print_str("[MAIN] Platform exit emitted\n");
    
    // Final error check
    if (code_buf.has_error) {
        print_str("[ERROR] Late buffer overflow detected!\n");
        return 1;
    }
    
    // Write executable based on target platform
    switch (target_platform) {
        case PLATFORM_WINDOWS:
            generate_pe_executable(code_buffer, code_buf.position, argv[2]);
            break;
        case PLATFORM_LINUX:
            generate_elf_executable(code_buffer, code_buf.position, argv[2]);
            break;
        case PLATFORM_MACOS:
            print_str("Error: macOS output not yet implemented\n");
            return 1;
        default:
            print_str("Error: Unknown platform\n");
            return 1;
    }
    
    print_str("Successfully compiled ");
    print_num(code_buf.position);
    print_str(" bytes\n");
    
    return 0;
}