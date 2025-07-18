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


// Forward declarations for SSE/float operations
extern void emit_movsd_xmm_mem(CodeBuffer* buf, SSERegister dst, X64Register base);
extern void emit_movsd_mem_xmm(CodeBuffer* buf, X64Register base, SSERegister src);
extern void generate_expression(CodeBuffer* buf, ASTNode* nodes, uint16_t expr_idx,
                               SymbolTable* symbols, char* string_pool);
extern bool is_float_expression_impl(ASTNode* nodes, uint16_t expr_idx, char* string_pool);
extern bool is_solid_expression_impl(ASTNode* nodes, uint16_t expr_idx, char* string_pool);

// Variable storage area
// We'll use a simple approach: allocate space on the stack for variables
// Variables are stored as name hash -> stack offset mapping

#define MAX_VARS 256
#define VAR_SIZE 8  // 64-bit values

typedef struct {
    uint32_t name_hash;
    int32_t stack_offset;  // Offset from original RSP (negative)
    bool is_initialized;
    uint8_t var_type;      // Variable type (VAR_TYPE_INT, VAR_TYPE_FLOAT, etc.)
} VarEntry;

// Variable type constants
#define VAR_TYPE_INT    0
#define VAR_TYPE_FLOAT  1
#define VAR_TYPE_STRING 2
#define VAR_TYPE_BOOL   3
#define VAR_TYPE_SOLID  4

// Global variable table (should be per-function in the future)
static VarEntry var_table[MAX_VARS];
static uint32_t var_count = 0;
static int32_t next_stack_offset = -8;  // Start at -8 from original RSP
static bool frame_setup = false;  // Track if we've set up the stack frame

// Simple hash function for variable names
static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + *str;
        str++;
    }
    return hash;
}

// Ensure stack frame is set up for variables
static void ensure_frame_setup(CodeBuffer* buf) {
    if (!frame_setup) {
        print_str("[VAR] Setting up stack frame on first variable use\n");
        // For main program, just allocate variable space without full frame setup
        // Don't push RBP or set it - we're not in a function
        // Reserve space for variables (256 bytes for 32 variables)
        emit_sub_reg_imm32(buf, RSP, 256);
        frame_setup = true;
    }
}

// Find or allocate a variable slot
VarEntry* get_or_create_var(const char* name) {
    uint32_t hash = hash_string(name);
    
    print_str("[VAR] get_or_create_var: name='");
    print_str(name);
    print_str("' hash=");
    print_num(hash);
    print_str("\n");
    
    // Look for existing variable
    for (uint32_t i = 0; i < var_count; i++) {
        if (var_table[i].name_hash == hash) {
            print_str("[VAR] Found existing var at index ");
            print_num(i);
            print_str(" type=");
            print_num(var_table[i].var_type);
            print_str("\n");
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
    // Don't emit anything - we'll set up the frame on first variable use
    // This avoids stack corruption for programs with no variables
}

// Clean up variable storage at function exit
void generate_var_storage_cleanup(CodeBuffer* buf) {
    // Only clean up if we set up a frame
    if (frame_setup) {
        print_str("[VAR_CLEANUP] Restoring stack frame\n");
        // For the main program (not in a function), we just need to restore RSP
        // We don't pop RBP because there's no caller to return to
        emit_add_reg_imm32(buf, RSP, 256);  // Clean up local variable space
        // Don't restore RBP or pop it - we're at the top level
    }
}

// Generate code to store a value in a variable
void generate_var_store(CodeBuffer* buf, const char* var_name, X64Register value_reg) {
    print_str("[VAR_STORE] Storing variable '");
    print_str(var_name);
    print_str("' from register ");
    print_num(value_reg);
    print_str("\n");
    
    // Ensure frame is set up before any variable operations
    ensure_frame_setup(buf);
    
    VarEntry* var = get_or_create_var(var_name);
    if (!var) return;  // Error: too many variables
    
    print_str("[VAR_STORE] Stack offset: ");
    print_num(var->stack_offset);
    print_str("\n");
    
    // Store value at [RSP + offset + 256] (since we allocated 256 bytes)
    // The offset is negative from the original RSP, but we've moved RSP down by 256
    // So actual offset from current RSP is 256 + var->stack_offset
    emit_mov_mem_reg(buf, RSP, 256 + var->stack_offset, value_reg);
    var->is_initialized = true;
}

// Generate code to load a variable value
void generate_var_load(CodeBuffer* buf, const char* var_name, X64Register dest_reg) {
    print_str("[VAR_LOAD] Loading variable '");
    print_str(var_name);
    print_str("' to register ");
    print_num(dest_reg);
    print_str("\n");
    
    // Ensure frame is set up before any variable operations
    ensure_frame_setup(buf);
    
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
    
    // Load value from [RSP + offset]
    // Note: If values have been pushed to the stack, we need to account for that
    // For now, we assume the caller handles stack adjustments
    emit_mov_reg_mem(buf, dest_reg, RSP, 256 + var->stack_offset);
}

// Generate code to store float variable from XMM0
void generate_var_store_float(CodeBuffer* buf, const char* var_name) {
    print_str("[VAR_STORE_FLOAT] Storing float variable '");
    print_str(var_name);
    print_str("' from XMM0\n");
    
    // Ensure frame is set up before any variable operations
    ensure_frame_setup(buf);
    
    VarEntry* var = get_or_create_var(var_name);
    if (!var) {
        print_str("[VAR_STORE_FLOAT] ERROR: Failed to get/create variable\n");
        return;
    }
    
    print_str("[VAR_STORE_FLOAT] Variable info: stack_offset=");
    print_num(var->stack_offset);
    print_str(" var_type=");
    print_num(var->var_type);
    print_str(" was_initialized=");
    print_num(var->is_initialized);
    print_str("\n");
    
    var->is_initialized = true;
    
    // Store XMM0 to [RSP + offset + 256]
    // movsd [rsp + offset], xmm0
    print_str("[VAR_STORE_FLOAT] Emitting movsd [rsp+");
    print_num(256 + var->stack_offset);
    print_str("], xmm0\n");
    
    emit_byte(buf, 0xF2); // SD prefix
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x11);
    emit_byte(buf, 0x84); // ModRM: [RSP + disp32]
    emit_byte(buf, 0x24); // SIB byte for RSP
    // Emit 32-bit displacement
    int32_t offset = 256 + var->stack_offset;
    emit_byte(buf, offset & 0xFF);
    emit_byte(buf, (offset >> 8) & 0xFF);
    emit_byte(buf, (offset >> 16) & 0xFF);
    emit_byte(buf, (offset >> 24) & 0xFF);
    
    print_str("[VAR_STORE_FLOAT] Float store complete\n");
}

// Generate code to load float variable into XMM0
void generate_var_load_float(CodeBuffer* buf, const char* var_name) {
    print_str("[VAR_LOAD_FLOAT] Loading float variable '");
    print_str(var_name);
    print_str("' to XMM0\n");
    
    // Ensure frame is set up before any variable operations
    ensure_frame_setup(buf);
    
    VarEntry* var = get_or_create_var(var_name);
    if (!var || !var->is_initialized) {
        // Variable not found or not initialized - load 0.0
        print_str("[VAR_LOAD_FLOAT] Variable not found or not initialized, loading 0.0\n");
        print_str("[VAR_LOAD_FLOAT] var=");
        print_num((unsigned long)var);
        if (var) {
            print_str(" is_initialized=");
            print_num(var->is_initialized);
            print_str(" var_type=");
            print_num(var->var_type);
        }
        print_str("\n");
        
        // xorpd xmm0, xmm0
        print_str("[VAR_LOAD_FLOAT] Emitting xorpd xmm0, xmm0\n");
        emit_byte(buf, 0x66);
        emit_byte(buf, 0x0F);
        emit_byte(buf, 0x57);
        emit_byte(buf, 0xC0);
        return;
    }
    
    print_str("[VAR_LOAD_FLOAT] Variable info: stack_offset=");
    print_num(var->stack_offset);
    print_str(" var_type=");
    print_num(var->var_type);
    print_str(" is_initialized=");
    print_num(var->is_initialized);
    print_str("\n");
    
    // Load from [RSP + offset + 256] to XMM0
    // movsd xmm0, [rsp + offset]
    print_str("[VAR_LOAD_FLOAT] Emitting movsd xmm0, [rsp+");
    print_num(256 + var->stack_offset);
    print_str("]\n");
    
    emit_byte(buf, 0xF2); // SD prefix
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x10);
    emit_byte(buf, 0x84); // ModRM: [RSP + disp32]
    emit_byte(buf, 0x24); // SIB byte for RSP
    // Emit 32-bit displacement
    int32_t offset = 256 + var->stack_offset;
    emit_byte(buf, offset & 0xFF);
    emit_byte(buf, (offset >> 8) & 0xFF);
    emit_byte(buf, (offset >> 16) & 0xFF);
    emit_byte(buf, (offset >> 24) & 0xFF);
    
    print_str("[VAR_LOAD_FLOAT] Float load complete\n");
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
    
    // Debug: print raw node data
    print_str("[VAR] Raw node data: ");
    uint8_t* raw = (uint8_t*)&node->data;
    for (int i = 0; i < 16; i++) {
        print_num(raw[i]);
        print_str(" ");
    }
    print_str("\n");
    
    uint32_t name_len = node->data.ident.name_len;
    uint16_t init_idx = node->data.timing.temporal_offset & 0xFFFF;  // Init expr stored in lower 16 bits
    
    // Get var_type from the node data (stored in upper bits of temporal_offset)
    uint8_t stored_var_type = (node->data.timing.temporal_offset >> 24) & 0xFF;
    char var_type = 'v';  // Default
    
    // Map stored var_type to character code
    switch (stored_var_type) {
        case 0: var_type = 'v'; break;  // generic var
        case 1: var_type = 'c'; break;  // const
        case 2: var_type = 'i'; break;  // int
        case 3: var_type = 'f'; break;  // float
        case 4: var_type = 's'; break;  // string
        case 5: var_type = 'b'; break;  // bool
        case 6: var_type = 'd'; break;  // solid
        case 7: var_type = 'c'; break;  // char
        default: var_type = 'v'; break;
    }
    
    // If no stored type, determine from init expression type
    if (stored_var_type == 0 && init_idx > 0 && init_idx < 4096) {
        ASTNode* init_node = &nodes[init_idx];
        if (init_node->type == NODE_FLOAT) {
            var_type = 'f';
        } else if (init_node->type == NODE_STRING) {
            var_type = 's';
        } else if (init_node->type == NODE_NUMBER) {
            var_type = 'i';
        } else if (init_node->type == NODE_SOLID) {
            var_type = 'd';  // 'd' for solid (can't use 's' - already used for string)
        }
    }
    
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
    
    // Debug: print string pool contents at offset
    print_str("[VAR] String pool at offset ");
    print_num(node->data.ident.name_offset);
    print_str(": ");
    for (uint32_t i = 0; i < 10 && i < name_len + 5; i++) {
        char c = string_pool[node->data.ident.name_offset + i];
        if (c >= 32 && c <= 126) {
            char buf[2] = {c, 0};
            print_str(buf);
        } else {
            print_str("[");
            print_num(c);
            print_str("]");
        }
    }
    print_str("\n");
    
    print_str("[VAR] generate_var_def_new: name='");
    print_str(var_name);
    print_str("' len=");
    print_num(name_len);
    print_str(" offset=");
    print_num(node->data.ident.name_offset);
    print_str("\n");
    
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
        print_str(", NODE_FLOAT=");
        print_num(NODE_FLOAT);
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
            print_str("[VAR] Processing NODE_FLOAT with var_name='");
            print_str(var_name);
            print_str("'\n");
            // Create float variable
            VarEntry* var = get_or_create_var_typed(var_name, VAR_TYPE_FLOAT);
            print_str("[VAR] Created float variable, type=");
            print_num(var ? var->var_type : -1);
            print_str("\n");
            // Generate expression to load float into XMM0
            generate_expression(buf, nodes, init_idx, symbols, string_pool);
            // Store float from XMM0
            generate_var_store_float(buf, var_name);
        } else if (init_node->type == NODE_SOLID) {
            print_str("[VAR] Processing NODE_SOLID with var_name='");
            print_str(var_name);
            print_str("'\n");
            // Create solid variable
            VarEntry* var = get_or_create_var_typed(var_name, VAR_TYPE_SOLID);
            print_str("[VAR] Created solid variable, type=");
            print_num(var ? var->var_type : -1);
            print_str("\n");
            // Generate the solid literal - this will put a pointer to the solid data in RAX
            generate_expression(buf, nodes, init_idx, symbols, string_pool);
            // Store the pointer in the variable
            generate_var_store(buf, var_name, RAX);
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
            
            // Check if the expression result is a float or solid
            bool is_float_expr = is_float_expression_impl(nodes, init_idx, string_pool);
            bool is_solid_expr = is_solid_expression_impl(nodes, init_idx, string_pool);
            
            print_str("[VAR] Expression is_float=");
            print_num(is_float_expr);
            print_str(" is_solid=");
            print_num(is_solid_expr);
            print_str("\n");
            
            // Create variable based on expression type (overrides var_type if needed)
            VarEntry* var = NULL;
            if (is_solid_expr || var_type == 'd') {
                print_str("[VAR] Creating SOLID variable due to is_solid_expr=");
                print_num(is_solid_expr);
                print_str(" or var_type=");
                print_num(var_type);
                print_str("\n");
                var = get_or_create_var_typed(var_name, VAR_TYPE_SOLID);
            } else if (is_float_expr || var_type == 'f') {
                print_str("[VAR] Creating FLOAT variable due to is_float_expr=");
                print_num(is_float_expr);
                print_str(" or var_type=");
                print_num(var_type);
                print_str("\n");
                var = get_or_create_var_typed(var_name, VAR_TYPE_FLOAT);
            } else {
                print_str("[VAR] Creating INT variable\n");
                var = get_or_create_var_typed(var_name, VAR_TYPE_INT);
            }
            
            // Generate the expression - result will be in RAX (int) or XMM0 (float)
            generate_expression(buf, nodes, init_idx, symbols, string_pool);
            
            // Store result based on actual expression type
            if (is_float_expr || var_type == 'f') {
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
    frame_setup = false;
}

// Check if a variable is a float type
bool is_var_float(const char* name) {
    VarEntry* var = get_or_create_var(name);
    return var && var->var_type == VAR_TYPE_FLOAT;
}

// Check if variable is a solid number
bool is_var_solid(const char* name) {
    VarEntry* var = get_or_create_var(name);
    return var && var->var_type == VAR_TYPE_SOLID;
}

// Check if any variables have been used
bool has_variables(void) {
    return var_count > 0;
}