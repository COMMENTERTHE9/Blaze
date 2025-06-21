// Symbol table type definitions
#ifndef SYMBOL_TABLE_TYPES_H
#define SYMBOL_TABLE_TYPES_H

#include "blaze_types.h"

// X64 register definitions
typedef enum {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8 = 8,  R9 = 9,  R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    RIP = 16  // Special register for RIP-relative addressing
} X64Register;

// Symbol entry
typedef struct {
    uint32_t name_hash;           // Hash of identifier name
    uint32_t name_offset;         // Offset into string pool
    uint16_t name_len;            // Length of name
    
    SymbolType type;
    StorageType storage;
    
    union {
        // Variable storage
        struct {
            X64Register reg;      // If in register
            int32_t stack_offset; // If on stack
            uint64_t value;       // If immediate
            bool is_mutable;
            bool is_temporal;     // Created by time-travel
        } var;
        
        // Function info
        struct {
            uint16_t ast_node;    // AST node for function body
            uint8_t param_count;
            uint8_t return_count;
            bool has_temporal_deps;
            uint32_t code_offset; // Offset in generated code
        } func;
        
        // 4D array info
        struct {
            uint32_t dimensions[4];
            uint64_t base_addr;
            bool is_temporal_indexed; // Can access time dimension
        } array_4d;
        
        // Jump label
        struct {
            uint32_t code_offset;
            bool is_resolved;
        } jump;
    } data;
    
    // Scope info
    uint16_t scope_level;
    uint16_t declaration_line;
    
    // Temporal info
    int32_t temporal_offset;      // When in time this exists
    bool visible_in_past;         // Can be seen before declaration
    bool visible_in_future;       // Can be seen after scope ends
} Symbol;

// Scope frame
typedef struct {
    uint16_t start_index;         // First symbol in this scope
    uint16_t symbol_count;        // Number of symbols
    uint16_t parent_scope;        // Parent scope index
    
    // Stack frame info
    int32_t stack_size;           // Total stack space used
    int32_t max_stack_offset;     // Deepest stack usage
    
    // Temporal scope info
    bool is_temporal_scope;       // Created by time-travel operation
    int32_t temporal_shift;       // Time offset of this scope
} ScopeFrame;

// Symbol table state
typedef struct SymbolTable {
    // Symbol storage
    Symbol symbols[128];          // Pre-allocated symbol array - reduced to avoid stack overflow
    uint16_t symbol_count;
    
    // Scope stack
    ScopeFrame scopes[64];        // Nested scopes
    uint16_t scope_count;
    uint16_t current_scope;
    
    // Register allocation
    uint8_t reg_alloc_mask;       // Bitmask of allocated registers
    uint8_t temp_reg_mask;        // Temporal registers (R12-R15)
    
    // Stack allocation
    int32_t current_stack_offset; // Current stack frame offset
    
    // String pool reference
    char* string_pool;
} SymbolTable;

#endif // SYMBOL_TABLE_TYPES_H