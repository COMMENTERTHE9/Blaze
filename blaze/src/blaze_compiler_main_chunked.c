// BLAZE COMPILER MAIN - Chunked arrays version
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
        : "rax", "rdi", "rsi", "rdx"
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
        : "rax", "rdi", "rsi", "rdx"
    );
    
    // Close file
    __asm__ volatile (
        "movq $3, %%rax\n"       // sys_close
        "movl %0, %%edi\n"       // fd
        "syscall\n"
        :
        : "r"(fd)
        : "rax", "rdi"
    );
    
    if (bytes_read < 0 || bytes_read > max_size) {
        return 0;
    }
    
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

// CHUNKED ARRAYS - Split large arrays into smaller segments
#define SOURCE_SEG 8192
#define TOK_SEG 512
#define NODE_SEG 512
#define CODE_SEG 8192

static char source_segments[4][SOURCE_SEG] = {0};        // 4 * 8K = 32K
static Token token_segments[8][TOK_SEG] = {0};           // 8 * 512 = 4096
static ASTNode node_segments[8][NODE_SEG] = {0};         // 8 * 512 = 4096
static char string_pool[4096] = {0};
static uint8_t code_segments[8][CODE_SEG] = {0};         // 8 * 8K = 64K
static ExecutionStep execution_plan[1024] = {0};
static SymbolTable symbols = {0};

// Macros to access chunked arrays
#define SOURCE(i) source_segments[(i)>>13][(i)&(SOURCE_SEG-1)]
#define TOK(i) token_segments[(i)>>9][(i)&(TOK_SEG-1)]
#define NODE(i) node_segments[(i)>>9][(i)&(NODE_SEG-1)]
#define CODE(i) code_segments[(i)>>13][(i)&(CODE_SEG-1)]

// Main compiler entry point
int main(int argc, char** argv) {
    write(1, "main() called\n", 14);
    
    // Validate arguments
    if (argc != 3) {
        print_str("Usage: ");
        print_str(argv[0]);
        print_str(" <input.blaze> <output>\n");
        return 1;
    }
    
    // Initialize first elements to avoid issues
    TOK(0).type = TOK_EOF;
    TOK(0).start = 0;
    TOK(0).len = 0;
    TOK(0).line = 0;
    
    // Initialize first node
    NODE(0).type = 0;
    
    // Initialize all nodes to ensure clean state
    for (int i = 0; i < 4096; i++) {
        NODE(i).type = 0;
        // Clear the data union
        for (int j = 0; j < sizeof(NODE(0).data); j++) {
            ((char*)&NODE(i).data)[j] = 0;
        }
    }
    
    // Read source file (use source_segments[0] as buffer)
    uint32_t source_len = read_file(argv[1], source_segments[0], 32767);
    if (source_len == 0) {
        return 1;
    }
    
    // Tokenize
    uint32_t token_count = lex_blaze(source_segments[0], source_len, &TOK(0));
    print_str("[MAIN] Lexer returned ");
    print_num(token_count);
    print_str(" tokens\n");
    
    if (token_count == 0) {
        print_str("Error: No tokens produced\n");
        return 1;
    }
    
    // Parse
    print_str("[MAIN] Starting parse with ");
    print_num(token_count);
    print_str(" tokens\n");
    print_str("[MAIN] Calling parse_blaze...\n");
    uint16_t root_idx = parse_blaze(&TOK(0), token_count, &NODE(0), 4096, string_pool, source_segments[0]);
    print_str("[MAIN] Parse returned root_idx=");
    print_num(root_idx);
    print_str("\n");
    
    if (root_idx == 0) {
        print_str("Error: Parse failed\n");
        return 1;
    }
    
    // Symbol table
    symbol_table_init(&symbols, string_pool);
    
    if (!build_symbol_table(&symbols, &NODE(0), root_idx, 4096, string_pool)) {
        print_str("Error: Symbol table build failed\n");
        return 1;
    }
    
    // Time travel analysis
    uint32_t plan_size = 0;
    if (!resolve_time_travel(&NODE(0), root_idx, 4096, string_pool, execution_plan)) {
        // No time travel needed - continue with normal compilation
        plan_size = 0;
    } else {
        // Count execution steps
        while (plan_size < 1024 && execution_plan[plan_size].node_idx != 0) {
            plan_size++;
        }
    }
    
    // Code generation
    CodeBuffer code_buf;
    code_buf.code = code_segments[0];  // Use first segment
    code_buf.position = 0;
    code_buf.capacity = MAX_CODE_SIZE;
    code_buf.temporal_count = 0;
    code_buf.entry_point = 0;
    code_buf.main_call_offset_pos = 0;
    code_buf.bss_offsets_need_patch = false;
    
    // Initialize runtime first
    generate_runtime_init_minimal(&code_buf);
    
    // Initialize variable storage before generating statements
    extern void generate_var_storage_init(CodeBuffer* buf);
    generate_var_storage_init(&code_buf);
    
    // Generate code
    generate_statement(&code_buf, &NODE(0), root_idx, &symbols, string_pool);
    
    // Clean up variable storage before exit
    extern void generate_var_storage_cleanup(CodeBuffer* buf);
    generate_var_storage_cleanup(&code_buf);
    
    // Exit cleanly
    emit_platform_exit(&code_buf, PLATFORM_LINUX, 0);
    
    // Write executable (gather segments if needed)
    uint8_t* final_code = code_segments[0];
    if (code_buf.position > CODE_SEG) {
        // TODO: Handle multi-segment code
        print_str("Error: Code too large for single segment\n");
        return 1;
    }
    
    generate_elf_executable(final_code, code_buf.position, argv[2]);
    
    return 0;
}