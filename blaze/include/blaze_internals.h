// BLAZE INTERNAL DEFINITIONS - No external dependencies

#ifndef BLAZE_INTERNALS_H
#define BLAZE_INTERNALS_H

#include <stdbool.h>
#include "blaze_types.h"
#include "symbol_table_types.h"
#include "blaze_stdlib.h"

// Memory management structures
// Reference count header (precedes each allocation)
typedef struct RCHeader {
    uint32_t size;
    uint16_t refcount;
    uint16_t flags;
    #define RC_FLAG_TEMPORAL  0x0001
    #define RC_FLAG_WEAK      0x0002
    #define RC_FLAG_ARRAY4D   0x0004
    #define RC_FLAG_MARKED    0x0008
} RCHeader;

// Temporal zone entry
typedef struct TemporalEntry {
    void* value_ptr;
    uint64_t timeline_id;
    int32_t temporal_offset;
    uint32_t creating_timeline;
    struct TemporalEntry* next;
    struct TemporalEntry* prev;
} TemporalEntry;

// Zone manager
typedef struct ZoneManager {
    TemporalEntry* entries;
    uint64_t used;
    uint64_t capacity;
    TimeZone zone_type;
} ZoneManager;

// Memory management globals
typedef struct MemoryState {
    void* arena;
    ZoneManager zones[3];
    uint8_t* heap_current;
    uint64_t total_allocated;
    uint64_t total_freed;
    bool initialized;
} MemoryState;

extern MemoryState g_memory;

// Configuration
#define MAX_TOKENS 4096
#define MAX_CODE_SIZE 65536   // 64KB - more reasonable for stack allocation
#define MAX_STACK_SIZE 1024

// System calls for Linux x64
#define SYS_WRITE 1
#define SYS_EXIT 60
#define SYS_MMAP 9
#define SYS_MUNMAP 11
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_READ 0

// File operation flags
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0100
#define O_TRUNC 01000

// Inline system call wrapper
static inline long syscall6(long num, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8 __asm__("r8") = a5;
    register long r9 __asm__("r9") = a6;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

// Character types for fast lexing
enum {
    CHAR_WHITESPACE = 1,
    CHAR_ALPHA = 2,
    CHAR_DIGIT = 3,
    CHAR_PIPE = 4,
    CHAR_SLASH = 5,
    CHAR_BACKSLASH = 6,
    CHAR_LT = 7,
    CHAR_GT = 8,
    CHAR_JUMP = 9,
    CHAR_BANG = 10,
    CHAR_COLON = 11,
    CHAR_STAR = 12,
    CHAR_MINUS = 13,
    CHAR_LBRACKET = 14,
    CHAR_RBRACKET = 15,
    CHAR_DOT = 16
};

// Token types
typedef enum {
    // Core operators
    TOK_LT,              // <
    TOK_GT,              // >
    TOK_TIMING_ONTO,     // <<
    TOK_TIMING_INTO,     // >>
    TOK_TIMING_BOTH,     // <>
    
    // Connectors
    TOK_CONNECTOR_FWD,   // \>|
    TOK_CONNECTOR_BWD,   // \<|
    
    // Actions
    TOK_ACTION_START,    // do/
    TOK_SLASH,           // forward slash
    TOK_BACKSLASH,       // backslash
    TOK_FUNC_CLOSE,      // colon-gt
    
    // Delimiters
    TOK_PIPE,            // |
    TOK_BRACKET_OPEN,    // [
    TOK_BRACKET_CLOSE,   // ]
    
    // Special
    TOK_JUMP_MARKER,     // ^
    TOK_GLOBAL_ERROR,    // !-N
    
    // Keywords
    TOK_VAR,             // var.v-
    TOK_CONST,           // var.c-
    TOK_VAR_INT,         // var.i-
    TOK_VAR_FLOAT,       // var.f-
    TOK_VAR_STRING,      // var.s-
    TOK_VAR_BOOL,        // var.b-
    TOK_VAR_SOLID,       // var.d-
    TOK_VAR_CHAR,        // var.char- or var.ch-
    TOK_ARRAY_4D,        // array.4d
    TOK_FUNC_CAN,        // fucn.can
    TOK_ERROR_CATCH,     // error.catch
    TOK_GAP_COMPUTE,     // gap.compute
    TOK_DECLARE,         // declare
    
    // Conditionals
    TOK_GREATER_THAN,    // *>
    TOK_LESS_EQUAL,      // *_<
    TOK_EQUAL,           // *=
    TOK_NOT_EQUAL,       // *!=
    
    // Basic
    TOK_IDENTIFIER,
    TOK_NUMBER,
    TOK_STRING,
    TOK_MINUS,           // -
    TOK_STAR,            // *
    TOK_COMMA,           // ,
    TOK_PLUS,            // +
    TOK_DIV,             // / (division, not TOK_SLASH)
    TOK_LT_CMP,          // < (comparison, not TOK_LT timing)
    TOK_GT_CMP,          // > (comparison, not TOK_GT timing)
    TOK_LE,              // <=
    TOK_GE,              // >=
    TOK_EQ,              // ==
    TOK_NE,              // !=
    
    // Additional Blaze tokens
    TOK_DOT,             // .
    TOK_UNDERSCORE,      // _
    TOK_AT,              // @
    TOK_SEMICOLON,       // ;
    TOK_PERCENT,         // %
    TOK_EQUALS,          // =
    TOK_LPAREN,          // (
    TOK_RPAREN,          // )
    TOK_LBRACE,          // {
    TOK_RBRACE,          // }
    TOK_COLON,           // :
    TOK_BANG,            // !
    TOK_COMMENT,         // ## comment ##
    // Parameter token
    TOK_PARAM,           // {@param:
    
    // Matrix tokens
    TOK_MATRIX_START,    // [:::
    
    // Conditional abbreviations
    TOK_COND_ENS,        // f.ens or fucn.ens
    
    // Solid number tokens
    TOK_SOLID_ELLIPSIS,  // ...
    TOK_SOLID_LPAREN,    // ( after ...
    TOK_SOLID_RPAREN,    // ) before ...
    TOK_SOLID_BARRIER,   // q, e, s, t, c, ∞, u
    TOK_SOLID_EXACT,     // exact
    TOK_SOLID_TERMINAL,  // terminal digits/symbols
    TOK_SOLID_PIPE,      // | in confidence
    TOK_SOLID_COLON,     // : in barrier spec
    TOK_SOLID_NUMBER,    // Complete solid number token
    TOK_COND_VER,        // f.ver or fucn.ver
    TOK_COND_CHK,        // f.chk or fucn.chk
    TOK_COND_TRY,        // f.try or fucn.try
    TOK_COND_GRD,        // f.grd or fucn.grd
    TOK_COND_UNL,        // f.unl or fucn.unl
    TOK_COND_IF,         // f.if or fucn.if
    TOK_COND_WHL,        // f.whl or fucn.whl
    TOK_COND_UNT,        // f.unt or fucn.unt
    TOK_COND_OBS,        // f.obs or fucn.obs
    TOK_COND_DET,        // f.det or fucn.det
    TOK_COND_REC,        // f.rec or fucn.rec
    TOK_COND_FS,         // f.fs or fucn.fs
    TOK_COND_RTE,        // f.rte or fucn.rte
    TOK_COND_MON,        // f.mon or fucn.mon
    TOK_COND_EVAL,       // f.eval or fucn.eval
    TOK_COND_DEC,        // f.dec or fucn.dec
    TOK_COND_ASS,        // f.ass or fucn.ass
    TOK_COND_MSR,        // f.msr or fucn.msr
    
    // Timeline tokens
    TOK_TIMELINE_DEF,    // timeline-[
    TOK_TIMELINE_JUMP,   // ^timeline.[
    TOK_BNC,             // bnc
    TOK_RECV,            // recv
    
    // Fixed point tokens
    TOK_FIX_P,           // fix.p
    TOK_F_P,             // f.p
    
    // Permanent timeline tokens
    TOK_TIMELINE_PER,    // timelineper-[
    TOK_TIMELINE_P,      // timelinep-[
    TOK_TIMELINE_P_JUMP, // ^timelinep.[
    
    // Action tokens  
    TOK_ACTION_CONTINUE, // /
    TOK_ACTION_END,      // backslash
    
    // Temporal operators
    TOK_BEFORE,          // <
    TOK_AFTER,           // >
    TOK_ONTO,            // <<
    TOK_INTO,            // >>
    TOK_BOTH,            // <>
    
    // Block end marker
    TOK_BLOCK_END,       // :>
    
    // Time-bridge operators
    TOK_TIME_BRIDGE_FWD, // >/>
    TOK_SLOW_FWD,        // >\>
    TOK_FAST_REWIND,     // </<
    TOK_SLOW_REWIND,     // <\<
    
    // Connectors
    TOK_FORWARD_CONN,    // \>|
    TOK_BACKWARD_CONN,   // \<|
    
    // Split tokens
    TOK_C_SPLIT,         // c.split._
    
    // Output method tokens
    TOK_PRINT,           // print/
    TOK_TXT,             // txt/
    TOK_OUT,             // out/
    TOK_FMT,             // fmt/
    TOK_DYN,             // dyn/
    
    // Assembly token
    TOK_ASM,             // asm/
    
    // Function call token
    TOK_FUNC_CALL,       // ^function_name (for stdlib calls)
    
    // Zone tokens
    TOK_PAST_ZONE,
    TOK_PRESENT_ZONE,
    TOK_FUTURE_ZONE,
    TOK_UNKNOWN_ZONE,
    
    // Logical operators
    TOK_AND,             // &&
    TOK_OR,              // ||
    
    // Bitwise operators (conflict-free)
    TOK_BIT_AND,         // &&.
    TOK_BIT_OR,          // ||.
    TOK_BIT_XOR,         // ^^
    TOK_BIT_NOT,         // ~~
    TOK_BIT_LSHIFT,      // <<<
    TOK_BIT_RSHIFT,      // >>>
    
    // Arithmetic operators
    TOK_EXPONENT,        // **
    
    // Math function prefix
    TOK_MATH_PREFIX,     // math.
    
    // Boolean literals
    TOK_TRUE,            // true
    TOK_FALSE,           // false
    
    // Control
    TOK_EOF,
    TOK_ERROR
} TokenType;

// Token structure - minimal size
typedef struct {
    TokenType type;
    uint32_t start;      // Position in source
    uint16_t len;        // Length of token
    uint16_t line;       // Line number for errors
} Token;

// X64Register is defined in symbol_table_types.h

// SSE register encoding (XMM0-XMM15)
typedef enum {
    XMM0 = 0, XMM1 = 1, XMM2 = 2, XMM3 = 3,
    XMM4 = 4, XMM5 = 5, XMM6 = 6, XMM7 = 7,
    XMM8 = 8, XMM9 = 9, XMM10 = 10, XMM11 = 11,
    XMM12 = 12, XMM13 = 13, XMM14 = 14, XMM15 = 15
} SSERegister;

// Platform types for cross-compilation
typedef enum {
    PLATFORM_LINUX,
    PLATFORM_WINDOWS,
    PLATFORM_MACOS
} Platform;

// Machine code buffer
typedef struct {
    uint8_t* code;
    uint32_t position;
    uint32_t capacity;
    bool has_error;     // Track buffer overflow errors
    
    // Time-travel state
    uint64_t temporal_markers[16];
    uint8_t temporal_count;
    
    // Entry point tracking for optimization fixes
    uint32_t entry_point;
    uint32_t main_call_offset_pos;
    bool bss_offsets_need_patch;
    
    // Target platform for code generation
    Platform target_platform;
} CodeBuffer;

// GGGX computation state
typedef struct {
    // Core metrics
    uint32_t debreading_efficiency;   // 0-1000 (scaled by 100)
    uint32_t parallel_potential;      // 0-1000
    uint32_t cluster_tightness;       // 0-1000
    
    // Confidence scores
    uint16_t confidence_d;            // 0-100
    uint16_t confidence_p;            // 0-100
    uint16_t confidence_c;            // 0-100
    
    // Results
    uint32_t gap_index;               // 0-1000
    uint32_t zone_score;              // Final score
    bool is_provisional;
} GGGX_State;

// AST node types
typedef enum {
    NODE_PROGRAM,
    NODE_VAR_DEF,
    NODE_FUNC_DEF,
    NODE_ACTION_BLOCK,
    NODE_DECLARE_BLOCK,
    NODE_TIMING_OP,
    NODE_CONDITIONAL,
    NODE_JUMP,
    NODE_EXPRESSION,
    NODE_BINARY_OP,
    NODE_NUMBER,
    NODE_FLOAT,
    NODE_IDENTIFIER,
    NODE_ARRAY_4D,
    NODE_ARRAY_4D_DEF,
    NODE_ARRAY_4D_ACCESS,
    NODE_GAP_ANALYSIS,
    NODE_GAP_COMPUTE,
    NODE_TIMELINE_DEF,
    NODE_TIMELINE_JUMP,
    NODE_FIXED_POINT,
    NODE_PERMANENT_TIMELINE,
    NODE_FLOW_SPEC,
    NODE_OUTPUT,
    NODE_STRING,
    NODE_INLINE_ASM,
    NODE_FUNC_CALL,
    NODE_UNARY_OP,
    NODE_SOLID,
    NODE_BOOL,
    NODE_RETURN
} NodeType;

// Define the maximum node type value
#define NODE_TYPE_MAX (NODE_RETURN + 1)

// AST Node - compact representation
typedef struct ASTNode {
    NodeType type;
    union {
        // Number literal (integer)
        int64_t number;
        
        // Float literal
        double float_value;
        
        // Identifier
        struct {
            uint32_t name_offset;
            uint16_t name_len;
        } ident;
        
        // Binary operation
        struct {
            TokenType op;
            uint16_t left_idx;
            uint16_t right_idx;
        } binary;
        
        // Time travel operation
        struct {
            TokenType timing_op;
            uint16_t expr_idx;
            int32_t temporal_offset;
        } timing;
        
        // 4D array
        struct {
            uint16_t name_idx;
            uint16_t dim_indices[4];
            uint16_t gap_analysis_idx;
        } array_4d;
        
        // GAP compute
        struct {
            uint16_t var_idx;           // Variable being computed
            uint16_t body_idx;          // Computation body
            uint16_t missing_list_idx;  // Missing data declarations
        } gap_compute;
        
        // Fixed point
        struct {
            uint16_t name_idx;          // Fixed point name
            uint16_t waiting_count;     // Number of timelines waiting
            uint16_t condition_idx;     // Optional condition
        } fixed_point;
        
        // Flow specification
        struct {
            uint16_t timeline_idx;      // Timeline being made permanent
            uint16_t rate;              // Rate in Hz (0 = unlimited)
            uint8_t flow_type;          // PERMANENT, RATE_LIMITED, etc
        } flow_spec;
        
        // Output operation
        struct {
            TokenType output_type;      // PRINT, TXT, OUT, FMT, DYN
            uint16_t content_idx;       // String or expression to output
            uint16_t next_output;       // For chained output methods
        } output;
        
        // Inline assembly
        struct {
            uint32_t code_offset;       // Offset in string pool for asm code
            uint16_t code_len;          // Length of assembly code
        } inline_asm;
        
        // Unary operation
        struct {
            TokenType op;               // Unary operator (!, ~, etc.)
            uint16_t expr_idx;          // Expression to apply operator to
        } unary;
        
        // Solid number
        struct {
            uint32_t known_offset;      // Offset in string pool for known digits
            uint16_t known_len;         // Length of known digits
            char barrier_type;          // 'q','e','s','t','c','∞','u','x' (x=exact)
            uint64_t gap_magnitude;     // 10^n or UINT64_MAX for infinity
            uint16_t confidence_x1000;  // Confidence * 1000 (for integer storage)
            uint32_t terminal_offset;   // Offset in string pool for terminal
            uint8_t terminal_len;       // Terminal digit count
            uint8_t terminal_type;      // 0=digits, 1=undefined(∅), 2=superposition({*})
        } solid;
        
        // Boolean value
        struct {
            bool value;                 // true or false
        } boolean;
    } data;
} ASTNode;

// Parser state structure
typedef struct Parser {
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

// Symbol table structures are defined in symbol_table_types.h

// Memory management structures (forward declarations only)
typedef struct TemporalMemory TemporalMemory;
typedef struct Array4D Array4D;
typedef struct RuntimeValueStruct RuntimeValue;

// Function prototypes
uint32_t lex_blaze(const char* input, uint32_t len, Token* output);
void emit_x64_instruction(CodeBuffer* buf, uint8_t* bytes, uint32_t len);
void analyze_gggx(Token* tokens, uint32_t count, GGGX_State* state);
uint16_t parse_blaze(Token* tokens, uint32_t count, ASTNode* node_pool, 
                     uint32_t pool_size, char* string_pool, const char* source);
bool resolve_time_travel(ASTNode* nodes, uint16_t root_idx, uint16_t node_count, 
                        char* string_pool, ExecutionStep* execution_plan);
void debug_print_ast(ASTNode* nodes, uint16_t root, char* string_pool);
bool build_symbol_table(SymbolTable* table, ASTNode* nodes, uint16_t root_idx,
                       uint16_t node_count, char* string_pool);
void debug_print_symbols(SymbolTable* table);

// Memory management functions
void temporal_memory_init(void* stack_base, uint32_t stack_size);
void* temporal_alloc_var(const char* name, uint32_t size, TimeZone zone);
void temporal_create_link(const char* var_name, TimeZone from_zone, TimeZone to_zone,
                         int32_t temporal_offset);
void* temporal_resolve_var(const char* name, bool needs_future_value);
Array4D* temporal_alloc_array4d(uint32_t x, uint32_t y, uint32_t z, uint32_t t,
                               uint32_t elem_size);
void* temporal_array4d_access(Array4D* arr, uint32_t x, uint32_t y, uint32_t z, uint32_t t);
MemoryPrediction temporal_predict_memory(ASTNode* nodes, uint16_t node_idx, 
                                       SymbolTable* symbols);
void temporal_memory_stats(uint32_t* past_used, uint32_t* present_used, 
                          uint32_t* future_used, uint16_t* link_count);

// Runtime functions
void runtime_init(uint32_t memory_size);
void runtime_store_value(const char* name, RuntimeValue* value, bool to_future);
RuntimeValue runtime_load_value(const char* name, bool from_future);
void runtime_debug_memory(void);

// Forward declarations for print functions
static inline void print_str(const char* str);
static inline void print_num(long num);

// Inline helpers for machine code emission
static inline void emit_byte(CodeBuffer* buf, uint8_t byte) {
    if (buf->position < buf->capacity) {
        buf->code[buf->position++] = byte;
    } else {
        // Buffer overflow - set error flag
        buf->has_error = true;
        // Optionally print error for debugging
        print_str("[EMIT] ERROR: Buffer overflow at position ");
        print_num(buf->position);
        print_str(" (capacity ");
        print_num(buf->capacity);
        print_str(")\n");
    }
}

static inline void emit_word(CodeBuffer* buf, uint16_t word) {
    emit_byte(buf, word & 0xFF);
    emit_byte(buf, (word >> 8) & 0xFF);
}

static inline void emit_dword(CodeBuffer* buf, uint32_t dword) {
    emit_byte(buf, dword & 0xFF);
    emit_byte(buf, (dword >> 8) & 0xFF);
    emit_byte(buf, (dword >> 16) & 0xFF);
    emit_byte(buf, (dword >> 24) & 0xFF);
}

static inline void emit_qword(CodeBuffer* buf, uint64_t qword) {
    emit_dword(buf, qword & 0xFFFFFFFF);
    emit_dword(buf, (qword >> 32) & 0xFFFFFFFF);
}

// X64 instruction encoding helpers
#define REX_W 0x48
#define REX_R 0x44
#define REX_X 0x42
#define REX_B 0x41

#define MODRM(mod, reg, rm) (((mod) << 6) | ((reg) << 3) | (rm))

// Simple print functions for debugging
static inline void print_str(const char* str) {
    const char* p = str;
    int len = 0;
    while (p[len]) len++;
    __asm__ volatile("push %%rcx; push %%r11;" ::: "memory");
    syscall6(SYS_WRITE, 1, (long)str, len, 0, 0, 0);
    __asm__ volatile("pop %%r11; pop %%rcx;" ::: "memory");
}

static inline void print_num(long num) {
    char buf[32];
    int i = 30;
    buf[31] = '\0';
    bool neg = false;
    if (num == 0) {
        buf[i--] = '0';
    } else {
        if (num < 0) {
            neg = true;
            num = -num;
        }
        while (num > 0 && i >= 0) {
            buf[i--] = '0' + (num % 10);
            num /= 10;
        }
        if (neg && i >= 0) buf[i--] = '-';
    }
    print_str(&buf[i + 1]);
}

// Additional function declarations for codegen
void emit_mov_reg_imm64(CodeBuffer* buf, X64Register reg, uint64_t value);
void emit_mov_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
void emit_mov_mem_reg(CodeBuffer* buf, X64Register base, int32_t offset, X64Register src);
void emit_mov_reg_mem(CodeBuffer* buf, X64Register dst, X64Register base, int32_t offset);
void emit_add_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
void emit_sub_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);

// Inline syscall_exit for use across modules
static inline void syscall_exit(int status) {
    __asm__ volatile(
        "push %%rax\n"
        "push %%rbx\n"
        "push %%rcx\n"
        "push %%rdx\n"
        "push %%rsi\n"
        "push %%rdi\n"
        "push %%rbp\n"
        "push %%r8\n"
        "push %%r9\n"
        "push %%r10\n"
        "push %%r11\n"
        "push %%r12\n"
        "push %%r13\n"
        "push %%r14\n"
        "push %%r15\n"
        "movl %0, %%edi\n"
        "movl $60, %%eax\n"
        "syscall\n"
        "pop %%r15\n"
        "pop %%r14\n"
        "pop %%r13\n"
        "pop %%r12\n"
        "pop %%r11\n"
        "pop %%r10\n"
        "pop %%r9\n"
        "pop %%r8\n"
        "pop %%rbp\n"
        "pop %%rdi\n"
        "pop %%rsi\n"
        "pop %%rdx\n"
        "pop %%rcx\n"
        "pop %%rbx\n"
        "pop %%rax\n"
        :
        : "r"(status)
        : "memory"
    );
    // Should not return
    __builtin_unreachable();
}
void emit_add_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
void emit_sub_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
void emit_mul_reg(CodeBuffer* buf, X64Register reg);
void emit_div_reg(CodeBuffer* buf, X64Register reg);
void emit_cmp_reg_reg(CodeBuffer* buf, X64Register r1, X64Register r2);
void emit_push_reg(CodeBuffer* buf, X64Register reg);
void emit_pop_reg(CodeBuffer* buf, X64Register reg);
void emit_rex(CodeBuffer* buf, bool w, bool r, bool x, bool b);
void emit_syscall(CodeBuffer* buf);
void emit_call_reg(CodeBuffer* buf, X64Register reg);
void emit_cmp_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
void emit_jmp_rel32(CodeBuffer* buf, int32_t offset);
void emit_je_rel32(CodeBuffer* buf, int32_t offset);
void emit_jne_rel32(CodeBuffer* buf, int32_t offset);
void emit_jg_rel32(CodeBuffer* buf, int32_t offset);
void emit_jle_rel32(CodeBuffer* buf, int32_t offset);
void emit_jge_rel32(CodeBuffer* buf, int32_t offset);
void emit_lea(CodeBuffer* buf, X64Register dst, X64Register base, int32_t offset);
void emit_xor_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
void emit_inc_reg(CodeBuffer* buf, X64Register reg);
void emit_test_reg_reg(CodeBuffer* buf, X64Register reg1, X64Register reg2);
void emit_jz(CodeBuffer* buf, int8_t offset);
void emit_jnz(CodeBuffer* buf, int8_t offset);
void emit_neg_reg(CodeBuffer* buf, X64Register reg);

// Memory initialization codegen
void generate_runtime_init(CodeBuffer* buf);
void generate_arena_alloc(CodeBuffer* buf, X64Register size_reg, X64Register result_reg);
void generate_rc_alloc(CodeBuffer* buf, X64Register size_reg, X64Register result_reg);
void generate_temporal_alloc(CodeBuffer* buf, TimeZone zone, X64Register size_reg, X64Register result_reg);
void generate_arena_enter_action(CodeBuffer* buf);
void generate_arena_exit_action(CodeBuffer* buf);
void generate_rc_inc(CodeBuffer* buf, X64Register ptr_reg);
void generate_rc_dec(CodeBuffer* buf, X64Register ptr_reg);

// Symbol table functions
void symbol_table_init(SymbolTable* table, char* string_pool);
void symbol_push_scope(SymbolTable* table, bool is_temporal, int32_t temporal_shift);
Symbol* symbol_lookup(SymbolTable* table, const char* name, uint16_t name_len, bool search_parent);
Symbol* symbol_add_array_4d(SymbolTable* table, const char* name, 
                           uint32_t x, uint32_t y, uint32_t z, uint32_t t);

// File I/O helpers
int syscall_open(const char* filename, int flags, int mode);
int syscall_close(int fd);
int syscall_write(int fd, volatile const void* buf, size_t count);
uint32_t str_len(const char* s);

// Platform utilities
const char* get_platform_name(Platform platform);

// Array4D functions
Array4D* array4d_create(uint32_t x, uint32_t y, uint32_t z, uint32_t t, size_t elem_size);
void array4d_set(Array4D* arr, int x, int y, int z, int t, void* value);
bool test_bit(uint8_t* map, size_t bit_idx);

// Memory management functions from memory_manager.c
void memory_init(void);
void* arena_alloc(uint64_t size);
void arena_enter_action(void);
void arena_exit_action(void);
void* rc_alloc(uint64_t size);
void rc_inc(void* ptr);
void rc_dec(void* ptr);
uint16_t rc_count(void* ptr);
void* temporal_alloc(TimeZone zone, uint64_t size);
void* temporal_move(void* ptr, TimeZone from_zone, TimeZone to_zone);
void memory_stats(void);
void temporal_gc(void);
void memory_test(void);

// Debug functions
void debug_print_tokens(Token* tokens, uint32_t count, const char* source);

// Variable type checking functions
bool is_var_float(const char* name);
bool is_var_solid(const char* name);

void generate_statement(CodeBuffer* buf, ASTNode* nodes, uint16_t stmt_idx, SymbolTable* symbols, char* string_pool);

#endif // BLAZE_INTERNALS_H