// FUNCTION DEFINITION AND CALL CODE GENERATION
// Handles |name| entry.can< :> function definitions and ^name/ calls

#include "blaze_internals.h"
#include "codegen_wrapper.h"

// Forward declarations
extern void generate_expression(CodeBuffer* buf, ASTNode* nodes, uint16_t expr_idx,
                              SymbolTable* symbols, char* string_pool);

// Function symbol table entry
typedef struct {
    uint32_t name_hash;
    uint32_t code_offset;  // Offset in code buffer where function starts
    uint16_t param_count;
    bool is_defined;
} FunctionEntry;

// Global function table (simple for now)
static FunctionEntry function_table[256];
static uint16_t function_count = 0;

// Fixup list for forward function references
typedef struct {
    uint32_t code_offset;    // Where in code buffer the call offset needs to be patched
    uint32_t name_hash;      // Hash of the function being called
} FunctionFixup;

static FunctionFixup fixup_list[256];
static uint16_t fixup_count = 0;

// Hash function name for lookup
static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + *str++;
    }
    return hash;
}

// Register or create function entry
static FunctionEntry* get_or_create_function(const char* name) {
    uint32_t hash = hash_string(name);
    
    // Linear search for now
    for (uint16_t i = 0; i < function_count; i++) {
        if (function_table[i].name_hash == hash) {
            return &function_table[i];
        }
    }
    
    // Create new entry
    if (function_count < 256) {
        FunctionEntry* entry = &function_table[function_count++];
        entry->name_hash = hash;
        entry->code_offset = 0;
        entry->param_count = 0;
        entry->is_defined = false;
        return entry;
    }
    return NULL;
}

// Generate GGGX function calls
void generate_gggx_function(CodeBuffer* buf, const char* func_name, uint16_t name_len,
                           ASTNode* nodes, uint16_t arg_idx, SymbolTable* symbols, char* string_pool) {
    print_str("[CODEGEN] Generating GGGX function: ");
    for (uint16_t i = 0; i < name_len && i < 32; i++) {
        print_num(func_name[i]);
        print_str(" ");
    }
    print_str("\n");
    
    // Generate arguments first (if any)
    if (arg_idx > 0 && arg_idx < 4096) {
        ASTNode* arg_node = &nodes[arg_idx];
        
        if (arg_node->type == NODE_BINARY_OP && arg_node->data.binary.op == TOK_COMMA) {
            // Multiple arguments (value, precision)
            uint16_t value_arg = arg_node->data.binary.left_idx;
            uint16_t precision_arg = arg_node->data.binary.right_idx;
            
            // Generate value argument
            if (value_arg > 0 && value_arg < 4096) {
                generate_expression(buf, nodes, value_arg, symbols, string_pool);
            }
            
            // Generate precision argument
            if (precision_arg > 0 && precision_arg < 4096) {
                generate_expression(buf, nodes, precision_arg, symbols, string_pool);
            }
        } else {
            // Single argument
            generate_expression(buf, nodes, arg_idx, symbols, string_pool);
        }
    }
    
    // Generate function call based on GGGX function name
    if (strncmp(func_name, "gggx_init", 9) == 0) {
        // Call gggx_init_engine()
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} // NOPs
        print_str("[CODEGEN] Stubbed gggx_init (NOP)\n");
        
    } else if (strncmp(func_name, "gggx_go", 7) == 0) {
        // Call gggx_go_phase_execute(value, precision)
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} print_str("[CODEGEN] Stubbed gggx_go (NOP)\n");
        
    } else if (strncmp(func_name, "gggx_get", 8) == 0) {
        // Call gggx_get_phase_execute(value, precision)
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} print_str("[CODEGEN] Stubbed gggx_get (NOP)\n");
        
    } else if (strncmp(func_name, "gggx_gap", 8) == 0) {
        // Call gggx_gap_phase_execute(value, precision)
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} print_str("[CODEGEN] Stubbed gggx_gap (NOP)\n");
        
    } else if (strncmp(func_name, "gggx_glimpse", 12) == 0) {
        // Call gggx_glimpse_phase_execute(value, precision)
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} print_str("[CODEGEN] Stubbed gggx_glimpse (NOP)\n");
        
    } else if (strncmp(func_name, "gggx_guess", 10) == 0) {
        // Call gggx_guess_phase_execute(value, precision)
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} print_str("[CODEGEN] Stubbed gggx_guess (NOP)\n");
        
    } else if (strncmp(func_name, "gggx_analyze_with_control", 24) == 0) {
        // Call gggx_analyze_with_control(value, precision)
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} print_str("[CODEGEN] Stubbed gggx_analyze_with_control (NOP)\n");
        
    } else if (strncmp(func_name, "gggx_set", 8) == 0) {
        // Call gggx_set_go_phase(function_name)
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} print_str("[CODEGEN] Stubbed gggx_set (NOP)\n");
        
    } else if (strncmp(func_name, "gggx_enable", 11) == 0) {
        // Call gggx_enable_phase(phase, enabled)
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} print_str("[CODEGEN] Stubbed gggx_enable (NOP)\n");
        
    } else if (strncmp(func_name, "gggx_status", 11) == 0) {
        // Call gggx_status_phase(phase)
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} print_str("[CODEGEN] Stubbed gggx_status (NOP)\n");
        
    } else if (strncmp(func_name, "gggx_print", 10) == 0) {
        // Call gggx_print_status()
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} print_str("[CODEGEN] Stubbed gggx_print (NOP)\n");
        
    } else if (strncmp(func_name, "gggx_alloc_trace", 16) == 0) {
        // Emit five NOPs (placeholder) instead of self-recursive CALL to avoid SIGFPE
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} // NOP NOP NOP NOP NOP
        print_str("[CODEGEN] Stubbed gggx_alloc_trace (NOP)\n");
    } else if (strncmp(func_name, "gggx_trace_access", 17) == 0) {
        // Emit five NOPs (placeholder) instead of self-recursive CALL to avoid SIGFPE
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} // NOPs
        print_str("[CODEGEN] Stubbed gggx_trace_access (NOP)\n");
    } else if (strncmp(func_name, "gggx_trace_activate", 19) == 0) {
        // Emit five NOPs (placeholder) instead of self-recursive CALL to avoid SIGFPE
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} // NOPs
        print_str("[CODEGEN] Stubbed gggx_trace_activate (NOP)\n");
    } else if (strncmp(func_name, "gggx_trace_deactivate", 21) == 0) {
        // Emit five NOPs (placeholder) instead of self-recursive CALL to avoid SIGFPE
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} // NOPs
        print_str("[CODEGEN] Stubbed gggx_trace_deactivate (NOP)\n");
    } else if (strncmp(func_name, "gggx_trace_cleanup_old", 22) == 0) {
        // Emit five NOPs (placeholder) instead of self-recursive CALL to avoid SIGFPE
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} // NOPs
        print_str("[CODEGEN] Stubbed gggx_trace_cleanup_old (NOP)\n");
    } else if (strncmp(func_name, "gggx_print_trace_stats", 23) == 0 || strncmp(func_name, "gggx_trace_stats", 17) == 0) {
        // Emit five NOPs (placeholder) instead of self-recursive CALL to avoid SIGFPE
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} // NOPs
        print_str("[CODEGEN] Stubbed gggx_trace_stats (NOP)\n");
    } else if (strncmp(func_name, "gggx_get_trace_id", 17) == 0) {
        // Emit five NOPs (placeholder) instead of self-recursive CALL to avoid SIGFPE
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} // NOPs
        print_str("[CODEGEN] Stubbed gggx_get_trace_id (NOP)\n");
    } else if (strncmp(func_name, "gggx_set_trace_complexity", 25) == 0) {
        // Emit five NOPs (placeholder) instead of self-recursive CALL to avoid SIGFPE
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} // NOPs
        print_str("[CODEGEN] Stubbed gggx_set_trace_complexity (NOP)\n");
    } else if (strncmp(func_name, "gggx_set_trace_confidence", 25) == 0) {
        // Emit five NOPs (placeholder) instead of self-recursive CALL to avoid SIGFPE
        for(int i=0;i<5;i++){emit_byte(buf,0x90);} // NOPs
        print_str("[CODEGEN] Stubbed gggx_set_trace_confidence (NOP)\n");
    } else {
        print_str("[CODEGEN] WARNING: Unknown GGGX function: ");
        for (uint16_t i = 0; i < name_len && i < 32; i++) {
            print_num(func_name[i]);
            print_str(" ");
        }
        print_str("\n");
    }
}

// Forward declaration for statement generation
extern void generate_statement(CodeBuffer* buf, ASTNode* nodes, uint16_t stmt_idx, 
                             SymbolTable* symbols, char* string_pool);

// Process fixups for a newly defined function
static void process_fixups_for_function(CodeBuffer* buf, uint32_t name_hash, uint32_t func_offset) {
    for (uint16_t i = 0; i < fixup_count; i++) {
        if (fixup_list[i].name_hash == name_hash) {
            // Calculate the relative offset from the call site to the function
            uint32_t call_site = fixup_list[i].code_offset;
            int32_t offset = func_offset - (call_site + 4); // +4 because offset is relative to end of instruction
            
            // Patch the offset in the code buffer
            uint8_t* patch_location = &buf->code[call_site];
            *(int32_t*)patch_location = offset;
            
            // Patched forward reference
            
            // Remove this fixup by moving the last one to this position
            fixup_list[i] = fixup_list[--fixup_count];
            i--; // Check this position again since we moved an item here
        }
    }
}

// Generate function prologue
static void emit_function_prologue(CodeBuffer* buf) {
    // push rbp
    emit_push_reg(buf, RBP);
    // mov rbp, rsp
    emit_mov_reg_reg(buf, RBP, RSP);
    // Optionally: sub rsp, N for local variables
}

// Generate function epilogue  
static void emit_function_epilogue(CodeBuffer* buf) {
    // mov rsp, rbp
    emit_mov_reg_reg(buf, RSP, RBP);
    // pop rbp
    emit_pop_reg(buf, RBP);
    // ret
    emit_byte(buf, 0xC3);
}

// Generate code for function definition
void generate_func_def(CodeBuffer* buf, ASTNode* nodes, uint16_t func_idx, 
                      SymbolTable* symbols, char* string_pool) {
    // Function definition generation
    
    if (func_idx == 0 || func_idx >= 4096) {
        print_str("  ERROR: Invalid function index\n");
        return;
    }
    
    ASTNode* func_node = &nodes[func_idx];
    if (func_node->type != NODE_FUNC_DEF) {
        print_str("  ERROR: Not a function definition node\n");
        return;
    }
    
    // Get function name from upper 16 bits of temporal_offset (as per parser fix)
    uint16_t name_idx = (func_node->data.timing.temporal_offset >> 16) & 0xFFFF;
    if (name_idx == 0 || name_idx >= 4096) {
        print_str("  ERROR: Invalid name index\n");
        return;
    }
    
    ASTNode* name_node = &nodes[name_idx];
    if (name_node->type != NODE_IDENTIFIER) {
        print_str("  ERROR: Function name is not an identifier\n");
        return;
    }
    
    const char* func_name = &string_pool[name_node->data.ident.name_offset];
    // Found function name
    
    // Get or create function entry
    FunctionEntry* entry = get_or_create_function(func_name);
    if (!entry) {
        print_str("  ERROR: Could not create function entry\n");
        return;
    }
    
    if (entry->is_defined) {
        print_str("  ERROR: Function '");
        print_str(func_name);
        print_str("' already defined\n");
        return;
    }
    
    // Mark function location
    entry->code_offset = buf->position;
    entry->is_defined = true;
    
    // Function starts here
    
    // Process any pending fixups for this function
    process_fixups_for_function(buf, entry->name_hash, entry->code_offset);
    
    // Generate function prologue
    emit_function_prologue(buf);
    
    // For now, no parameters (functions are simple)
    print_str("  Function has no parameters\n");
    
    // Get function body from left_idx (as per parser structure)
    uint16_t body_idx = func_node->data.binary.left_idx;
    print_str("  Function body_idx: ");
    print_num(body_idx);
    print_str("\n");
    
    if (body_idx != 0 && body_idx < 4096) {
        // Process function body
        
        // Generate code for function body
        // The body is an action block, which is a chain of statements
        generate_statement(buf, nodes, body_idx, symbols, string_pool);
    } else {
        print_str("  Function has empty body\n");
    }
    
    // Generate function epilogue
    emit_function_epilogue(buf);
    
    // Function generation complete
}

// Forward declaration for math function support
extern bool is_math_function(const char* name, uint16_t len);
extern void generate_math_function(CodeBuffer* buf, const char* func_name, uint16_t name_len,
                                  ASTNode* nodes, uint16_t arg_idx,
                                  SymbolTable* symbols, char* string_pool);

// Generate code for function call
void generate_func_call(CodeBuffer* buf, ASTNode* nodes, uint16_t call_idx,
                       SymbolTable* symbols, char* string_pool) {
    // Function call generation
    print_str("[CODEGEN] Generating function call at node_idx=");
    print_num(call_idx);
    print_str("\n");
    
    if (call_idx == 0 || call_idx >= 4096) {
        print_str("  ERROR: Invalid call index\n");
        return;
    }
    
    ASTNode* call_node = &nodes[call_idx];
    if (call_node->type != NODE_FUNC_CALL) {
        print_str("  ERROR: Not a function call node\n");
        return;
    }
    
    // Get function name from left_idx
    uint16_t name_idx = call_node->data.binary.left_idx;
    if (name_idx == 0 || name_idx >= 4096) {
        print_str("  ERROR: Invalid name index in call\n");
        return;
    }
    
    ASTNode* name_node = &nodes[name_idx];
    if (name_node->type != NODE_IDENTIFIER) {
        print_str("  ERROR: Function name is not an identifier\n");
        return;
    }
    
    const char* func_name = &string_pool[name_node->data.ident.name_offset];
    uint16_t name_len = name_node->data.ident.name_len;
    
    print_str("[CODEGEN] Function call: ");
    for (uint16_t i = 0; i < name_len && i < 32; i++) {
        print_num(func_name[i]);
        print_str(" ");
    }
    print_str(" (len=");
    print_num(name_len);
    print_str(")\n");
    
    // Check if this is a math function
    if (is_math_function(func_name, name_len)) {
        // Handle math functions specially
        // The argument is in right_idx
        uint16_t arg_idx = call_node->data.binary.right_idx;
        generate_math_function(buf, func_name, name_len, nodes, arg_idx, symbols, string_pool);
        return;
    }
    
    // Check if this is a GGGX function
    if (strncmp(func_name, "gggx_", 5) == 0) {
        print_str("[CODEGEN] GGGX function call: ");
        for (uint16_t i = 0; i < name_len && i < 32; i++) {
            print_num(func_name[i]);
            print_str(" ");
        }
        print_str("\n");
        
        // Handle GGGX functions specially
        uint16_t arg_idx = call_node->data.binary.right_idx;
        generate_gggx_function(buf, func_name, name_len, nodes, arg_idx, symbols, string_pool);
        return;
    }
    
    // Calling function
    
    // Look up function
    FunctionEntry* entry = get_or_create_function(func_name);
    if (!entry) {
        print_str("  ERROR: Could not find/create function entry\n");
        return;
    }
    
    print_str("[CODEGEN] Found function entry, generating call\n");
    
    // Save volatile registers per System V ABI
    emit_push_reg(buf, RAX);
    emit_push_reg(buf, RCX);
    emit_push_reg(buf, RDX);
    emit_push_reg(buf, RSI);
    emit_push_reg(buf, RDI);
    emit_push_reg(buf, R8);
    emit_push_reg(buf, R9);
    emit_push_reg(buf, R10);
    emit_push_reg(buf, R11);
    
    // We pushed 9 registers (72 bytes). With the return address (8 bytes) on stack,
    // we have 80 bytes total, which is NOT 16-byte aligned.
    // Add 8 bytes to make it 88 bytes (divisible by 16)
    emit_sub_reg_imm32(buf, RSP, 8);
    
    // For now, no parameters (functions are simple)
    print_str("[CODEGEN] No parameters for function call\n");
    
    if (entry->is_defined) {
        // Function is already defined, calculate relative offset
        // The call instruction is 5 bytes: 0xE8 + 4-byte offset
        // The offset is calculated from the END of the call instruction
        int32_t offset = entry->code_offset - (buf->position + 5);
        
        print_str("[CODEGEN] Function is defined, offset=");
        print_num(offset);
        print_str("\n");
        
        emit_byte(buf, 0xE8); // CALL rel32
        emit_dword(buf, offset);
    } else {
        // Forward reference - will fixup later
        
        print_str("[CODEGEN] Function is forward reference, creating fixup\n");
        
        // Emit call instruction with placeholder
        emit_byte(buf, 0xE8); // CALL rel32
        uint32_t fixup_location = buf->position;
        emit_dword(buf, 0); // Placeholder offset
        
        // Add to fixup list
        if (fixup_count < 256) {
            fixup_list[fixup_count].code_offset = fixup_location;
            fixup_list[fixup_count].name_hash = entry->name_hash;
            fixup_count++;
        }
    }
    
    // Remove alignment padding
    emit_add_reg_imm32(buf, RSP, 8);
    
    // Restore volatile registers
    emit_pop_reg(buf, R11);
    emit_pop_reg(buf, R10);
    emit_pop_reg(buf, R9);
    emit_pop_reg(buf, R8);
    emit_pop_reg(buf, RDI);
    emit_pop_reg(buf, RSI);
    emit_pop_reg(buf, RDX);
    emit_pop_reg(buf, RCX);
    emit_pop_reg(buf, RAX);
    
    print_str("[CODEGEN] Function call generation complete\n");
    // Function call complete
}