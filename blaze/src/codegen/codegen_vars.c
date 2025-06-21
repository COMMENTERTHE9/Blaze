// Variable storage and retrieval code generation

#include "blaze_internals.h"

// Forward declarations
extern void emit_byte(CodeBuffer* buf, uint8_t byte);
extern void emit_mov_reg_imm64(CodeBuffer* buf, X64Register reg, uint64_t value);
extern void emit_mov_reg_mem(CodeBuffer* buf, X64Register dst, X64Register base, int32_t offset);
extern void emit_mov_mem_reg(CodeBuffer* buf, X64Register base, int32_t offset, X64Register src);
extern void emit_lea(CodeBuffer* buf, X64Register dst, X64Register base, int32_t offset);
extern void emit_push_reg(CodeBuffer* buf, X64Register reg);
extern void emit_pop_reg(CodeBuffer* buf, X64Register reg);
extern void emit_mov_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_sub_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_sub_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);

// SSE register definitions
typedef enum {
    XMM0 = 0, XMM1 = 1, XMM2 = 2, XMM3 = 3,
    XMM4 = 4, XMM5 = 5, XMM6 = 6, XMM7 = 7
} SSERegister;

// Forward declarations for SSE/float operations
extern void emit_movsd_xmm_mem(CodeBuffer* buf, SSERegister dst, X64Register base);
extern void emit_movsd_mem_xmm(CodeBuffer* buf, X64Register base, SSERegister src);
extern void generate_expression(CodeBuffer* buf, ASTNode* nodes, uint16_t expr_idx,
                               SymbolTable* symbols, char* string_pool);

// Variable storage area
// We'll use a simple approach: allocate space on the stack for variables
// Variables are stored as name hash -> stack offset mapping

#define MAX_VARS 256
#define VAR_SIZE 8  // 64-bit values

typedef struct {
    uint32_t name_hash;
    int32_t stack_offset;  // Offset from RBP
    bool is_initialized;
    uint8_t var_type;      // Variable type (VAR_TYPE_INT, VAR_TYPE_FLOAT, etc.)
} VarEntry;

// Variable type constants
#define VAR_TYPE_INT    0
#define VAR_TYPE_FLOAT  1
#define VAR_TYPE_STRING 2
#define VAR_TYPE_BOOL   3

// Global variable table (should be per-function in the future)
static VarEntry var_table[MAX_VARS];
static uint32_t var_count = 0;
static int32_t next_stack_offset = -8;  // Start after saved RBP

// Simple hash function for variable names
static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + *str;
        str++;
    }
    return hash;
}

// Find or allocate a variable slot
static VarEntry* get_or_create_var(const char* name) {
    uint32_t hash = hash_string(name);
    
    // Debug output disabled
    // print_str("[VAR] get_or_create_var: name='");
    // print_str(name);
    // print_str("' hash=");
    // print_num(hash);
    // print_str("\n");
    
    // Look for existing variable
    for (uint32_t i = 0; i < var_count; i++) {
        if (var_table[i].name_hash == hash) {
            return &var_table[i];
        }
    }
    
    // Create new variable
    if (var_count < MAX_VARS) {
        VarEntry* var = &var_table[var_count++];
        var->name_hash = hash;
        var->stack_offset = next_stack_offset;
        var->is_initialized = false;
        var->var_type = VAR_TYPE_INT;  // Default to int
        next_stack_offset -= VAR_SIZE;
        return var;
    }
    
    return NULL;  // Too many variables
}

// Find or allocate a variable slot with type
static VarEntry* get_or_create_var_typed(const char* name, uint8_t type) {
    VarEntry* var = get_or_create_var(name);
    if (var && !var->is_initialized) {
        var->var_type = type;
    }
    return var;
}

// Initialize variable storage at function entry
void generate_var_storage_init(CodeBuffer* buf) {
    // RBP is pushed by runtime init, but we need to set it up as frame pointer
    emit_mov_reg_reg(buf, RBP, RSP);
    
    // Reserve space for variables (256 bytes for 32 variables)
    emit_sub_reg_imm32(buf, RSP, 256);
}

// Clean up variable storage at function exit
void generate_var_storage_cleanup(CodeBuffer* buf) {
    // Just restore stack pointer by adding back the space we allocated
    emit_add_reg_imm32(buf, RSP, 256);
}

// Generate code to store a value in a variable
void generate_var_store(CodeBuffer* buf, const char* var_name, X64Register value_reg) {
    print_str("[VAR_STORE] Storing variable '");
    print_str(var_name);
    print_str("' from register ");
    print_num(value_reg);
    print_str("\n");
    
    VarEntry* var = get_or_create_var(var_name);
    if (!var) return;  // Error: too many variables
    
    print_str("[VAR_STORE] Stack offset: ");
    print_num(var->stack_offset);
    print_str("\n");
    
    // Store value at [RBP + offset]
    emit_mov_mem_reg(buf, RBP, var->stack_offset, value_reg);
    var->is_initialized = true;
}

// Generate code to load a variable value
void generate_var_load(CodeBuffer* buf, const char* var_name, X64Register dest_reg) {
    print_str("[VAR_LOAD] Loading variable '");
    print_str(var_name);
    print_str("' to register ");
    print_num(dest_reg);
    print_str("\n");
    
    VarEntry* var = get_or_create_var(var_name);
    if (!var) {
        // Variable not found - load 0 as default
        print_str("[VAR_LOAD] Variable not found! Loading 0\n");
        emit_mov_reg_imm64(buf, dest_reg, 0);
        return;
    }
    
    print_str("[VAR_LOAD] Stack offset: ");
    print_num(var->stack_offset);
    print_str(" initialized: ");
    print_num(var->is_initialized);
    print_str("\n");
    
    // Load value from [RBP + offset]
    emit_mov_reg_mem(buf, dest_reg, RBP, var->stack_offset);
}

// Generate code to store float variable from XMM0
void generate_var_store_float(CodeBuffer* buf, const char* var_name) {
    VarEntry* var = get_or_create_var(var_name);
    if (!var) return;
    
    var->is_initialized = true;
    
    // Store XMM0 to [RBP + offset]
    // movsd [rbp + offset], xmm0
    emit_byte(buf, 0xF2); // SD prefix
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x11);
    emit_byte(buf, 0x85); // ModRM: [RBP + disp32]
    // Emit 32-bit displacement
    int32_t offset = var->stack_offset;
    emit_byte(buf, offset & 0xFF);
    emit_byte(buf, (offset >> 8) & 0xFF);
    emit_byte(buf, (offset >> 16) & 0xFF);
    emit_byte(buf, (offset >> 24) & 0xFF);
}

// Generate code to load float variable into XMM0
void generate_var_load_float(CodeBuffer* buf, const char* var_name) {
    VarEntry* var = get_or_create_var(var_name);
    if (!var || !var->is_initialized) {
        // Variable not found or not initialized - load 0.0
        // xorpd xmm0, xmm0
        emit_byte(buf, 0x66);
        emit_byte(buf, 0x0F);
        emit_byte(buf, 0x57);
        emit_byte(buf, 0xC0);
        return;
    }
    
    // Load from [RBP + offset] to XMM0
    // movsd xmm0, [rbp + offset]
    emit_byte(buf, 0xF2); // SD prefix
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x10);
    emit_byte(buf, 0x85); // ModRM: [RBP + disp32]
    // Emit 32-bit displacement
    int32_t offset = var->stack_offset;
    emit_byte(buf, offset & 0xFF);
    emit_byte(buf, (offset >> 8) & 0xFF);
    emit_byte(buf, (offset >> 16) & 0xFF);
    emit_byte(buf, (offset >> 24) & 0xFF);
}

// Generate code for variable definition with initialization
void generate_var_def_new(CodeBuffer* buf, ASTNode* nodes, uint16_t node_idx, 
                          SymbolTable* symbols, char* string_pool) {
    
    if (!buf || !nodes || !string_pool) {
        print_str("[VAR] ERROR: NULL pointer passed to generate_var_def_new\n");
        return;
    }
    
    if (node_idx == 0 || node_idx >= 4096) {
        print_str("[VAR] ERROR: Invalid node_idx=");
        print_num(node_idx);
        print_str("\n");
        return;
    }
    
    // Debug: Print node pointer for sanity check
    print_str("[VAR] nodes pointer: ");
    print_num((unsigned long)nodes);
    print_str(" node_idx: ");
    print_num(node_idx);
    print_str("\n");
    
    ASTNode* node = &nodes[node_idx];
    
    // Debug: Check node type
    if (node->type != NODE_VAR_DEF) {
        print_str("[VAR] ERROR: Not a VAR_DEF node, type=");
        print_num(node->type);
        print_str(" at index ");
        print_num(node_idx);
        print_str("\n");
        return;
    }
    
    // Get variable name
    char var_name[256];
    uint32_t name_len = node->data.ident.name_len;
    uint16_t init_idx = node->data.timing.temporal_offset;  // Init expr stored here
    uint8_t var_type = 'v';  // Default to generic var for now
    
    if (name_len == 0 || name_len > 255) {
        print_str("[VAR] ERROR: Invalid name_len=");
        print_num(name_len);
        print_str("\n");
        return;
    }
    
    print_str("[VAR] Reading from node: name_len=");
    print_num(name_len);
    print_str(" var_type=");
    print_num(var_type);
    print_str(" init_idx=");
    print_num(init_idx);
    print_str(" name_offset=");
    print_num(node->data.ident.name_offset);
    print_str("\n");
    
    // Check name_offset validity
    if (node->data.ident.name_offset >= 4096) {
        print_str("[VAR] ERROR: Invalid name_offset\n");
        return;
    }
    
    for (uint32_t i = 0; i < name_len; i++) {
        var_name[i] = string_pool[node->data.ident.name_offset + i];
    }
    var_name[name_len] = '\0';
    
    // Debug output disabled
    // print_str("[VAR] generate_var_def_new: name='");
    // print_str(var_name);
    // print_str("' len=");
    // print_num(name_len);
    // print_str(" offset=");
    // print_num(node->data.ident.name_offset);
    // print_str("\n");
    
    print_str("\n[VAR] init_idx=");
    print_num(init_idx);
    print_str("\n");
    
    if (init_idx > 0 && init_idx < 4096) {
        print_str("[VAR] Entering init block with init_idx=");
        print_num(init_idx);
        print_str("\n");
        
        ASTNode* init_node = &nodes[init_idx];
        
        print_str("[VAR] init_node type=");
        print_num(init_node->type);
        print_str(" (NODE_NUMBER=");
        print_num(NODE_NUMBER);
        print_str(")\n");
        
        if (init_node->type == NODE_NUMBER) {
            print_str("[VAR] Initializing with number value=");
            print_num(init_node->data.number);
            print_str("\n");
            
            // Create typed variable based on var_type
            VarEntry* var = NULL;
            if (var_type == 'i' || var_type == 'v') {
                var = get_or_create_var_typed(var_name, VAR_TYPE_INT);
            }
            // Load immediate value into RAX
            emit_mov_reg_imm64(buf, RAX, init_node->data.number);
            // Store in variable
            generate_var_store(buf, var_name, RAX);
        } else if (init_node->type == NODE_FLOAT) {
            // Create float variable
            VarEntry* var = get_or_create_var_typed(var_name, VAR_TYPE_FLOAT);
            // Generate expression to load float into XMM0
            generate_expression(buf, nodes, init_idx, symbols, string_pool);
            // Store float from XMM0
            generate_var_store_float(buf, var_name);
        } else if (init_node->type == NODE_STRING) {
            // Create string variable
            VarEntry* var = get_or_create_var_typed(var_name, VAR_TYPE_STRING);
            // For now, store string address
            // TODO: Implement proper string storage
            emit_mov_reg_imm64(buf, RAX, 0);
            generate_var_store(buf, var_name, RAX);
        } else {
            // Handle any other expression (like binary operations)
            print_str("[VAR] Initializing with expression\n");
            
            // Create variable based on type
            VarEntry* var = NULL;
            if (var_type == 'i' || var_type == 'v') {
                var = get_or_create_var_typed(var_name, VAR_TYPE_INT);
            } else if (var_type == 'f') {
                var = get_or_create_var_typed(var_name, VAR_TYPE_FLOAT);
            }
            
            // Generate the expression - result will be in RAX (int) or XMM0 (float)
            generate_expression(buf, nodes, init_idx, symbols, string_pool);
            
            // Store result based on type
            if (var_type == 'f') {
                generate_var_store_float(buf, var_name);
            } else {
                generate_var_store(buf, var_name, RAX);
            }
        }
    } else {
        // No initializer - store 0
        emit_mov_reg_imm64(buf, RAX, 0);
        generate_var_store(buf, var_name, RAX);
    }
}

// Generate code for identifier (variable load)
void generate_identifier(CodeBuffer* buf, ASTNode* nodes, uint16_t node_idx,
                        SymbolTable* symbols, char* string_pool) {
    print_str("[IDENT] generate_identifier called with node_idx=");
    print_num(node_idx);
    print_str("\n");
    
    ASTNode* node = &nodes[node_idx];
    
    // Get variable name
    char var_name[256];
    uint32_t name_len = node->data.ident.name_len;
    if (name_len >= 256) name_len = 255;
    
    for (uint32_t i = 0; i < name_len; i++) {
        var_name[i] = string_pool[node->data.ident.name_offset + i];
    }
    var_name[name_len] = '\0';
    
    // Check variable type
    VarEntry* var = get_or_create_var(var_name);
    if (var && var->var_type == VAR_TYPE_FLOAT) {
        // Load float variable into XMM0
        generate_var_load_float(buf, var_name);
    } else {
        // Load integer/other variable into RAX
        generate_var_load(buf, var_name, RAX);
    }
}

// Reset variable table for new function
void reset_var_table(void) {
    var_count = 0;
    next_stack_offset = -8;
}