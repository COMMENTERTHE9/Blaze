// BLAZE COMPILER MAIN - Clean version without Sentry
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
extern void gen_output_method(uint8_t* output, uint32_t* offset, ASTNode* node,
                             char* string_pool, SymbolTable* symbols);
extern void gen_inline_asm(uint8_t* output, uint32_t* offset, ASTNode* node,
                          char* string_pool, SymbolTable* symbols);
extern void generate_runtime_init_minimal(CodeBuffer* buf);

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
        "movq %3, %%rdx\n"       // max_size
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
        : "rax", "rdi", "rcx", "r11", "memory"
    );
    
    if (bytes_read < 0 || bytes_read > max_size) {
        print_str("Error: Failed to read file\n");
        return 0;
    }
    
    buffer[bytes_read] = '\0';
    return bytes_read;
}

// Global static buffers to avoid stack issues
// Mark as volatile to prevent optimization issues
static volatile char source_buffer[32768] = {0};
static volatile Token tokens[MAX_TOKENS] = {0};
static volatile ASTNode nodes[4096] = {0};
static volatile char string_pool[4096] = {0};
static volatile uint8_t code_buffer[MAX_CODE_SIZE] = {0};
static volatile ExecutionStep execution_plan[1024] = {0};
static volatile SymbolTable symbols = {0};

// Main compiler entry point
int main(int argc, char** argv) {
    write(1, "main() called\n", 14);
    
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
    if (argc != 3) {
        const char* usage = "Usage: blaze <input.blaze> <output>\n";
        write(1, usage, str_len(usage));
        return 1;
    }
    
    // Read source file
    uint32_t source_len = read_file(argv[1], (char*)source_buffer, 32767);
    if (source_len == 0) {
        return 1;
    }
    
    // Tokenize
    uint32_t token_count = lex_blaze((const char*)source_buffer, source_len, (Token*)tokens);
    if (token_count == 0) {
        print_str("Error: No tokens generated\n");
        return 1;
    }
    
    // Parse
    print_str("[MAIN] Starting parse with ");
    print_num(token_count);
    print_str(" tokens\n");
    print_str("[MAIN] Calling parse_blaze...\n");
    uint16_t root_idx = parse_blaze((Token*)tokens, token_count, (ASTNode*)nodes, 
                                    4096, (char*)string_pool, (const char*)source_buffer);
    print_str("[MAIN] Parse returned root_idx=");
    print_num(root_idx);
    print_str("\n");
    if (root_idx == 0) {
        print_str("Error: Parse failed\n");
        return 1;
    }
    
    // Check for type 243 error
    for (uint16_t i = 0; i < 100 && nodes[i].type != 0; i++) {
        if (nodes[i].type == 243) {
            print_str("[ERROR] Found type 243 at node ");
            print_num(i);
            print_str("\n");
            uint64_t* ptr = (uint64_t*)&nodes[i];
            print_str("Node data: ");
            for (int j = 0; j < 4; j++) {
                print_num(ptr[j]);
                print_str(" ");
            }
            print_str("\n");
        }
    }
    
    // Symbol table
    print_str("[MAIN] Initializing symbol table\n");
    symbol_table_init((SymbolTable*)&symbols, (char*)string_pool);
    
    print_str("[MAIN] Building symbol table\n");
    if (!build_symbol_table((SymbolTable*)&symbols, (ASTNode*)nodes, root_idx, 4096, (char*)string_pool)) {
        print_str("Error: Symbol table build failed\n");
        return 1;
    }
    print_str("[MAIN] Symbol table built successfully\n");
    
    // Time travel analysis
    uint32_t plan_size = 0;
    if (!resolve_time_travel((ASTNode*)nodes, root_idx, 4096, (char*)string_pool, (ExecutionStep*)execution_plan)) {
        // No time travel needed - continue with normal compilation
        plan_size = 0;
    } else {
        // Count execution steps
        while (plan_size < 1024 && execution_plan[plan_size].node_idx != 0) {
            plan_size++;
        }
    }
    
    // Generate machine code
    CodeBuffer code_buf = {
        .code = (uint8_t*)code_buffer,
        .position = 0,
        .capacity = MAX_CODE_SIZE
    };
    
    // Initialize runtime with minimal setup
    generate_runtime_init_minimal(&code_buf);
    
    // Generate code
    generate_statement(&code_buf, (ASTNode*)nodes, root_idx, (SymbolTable*)&symbols, (char*)string_pool);
    
    // Add exit
    emit_platform_exit(&code_buf, PLATFORM_LINUX, 0);
    
    print_str("Generated ");
    print_num(code_buf.position);
    print_str(" bytes of machine code\n");
    
    // Generate executable
    generate_elf_executable(code_buf.code, code_buf.position, argv[2]);
    
    print_str("Executable written to ");
    print_str(argv[2]);
    print_str("\n");
    
    return 0;
}