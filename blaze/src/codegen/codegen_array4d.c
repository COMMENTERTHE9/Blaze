// 4D ARRAY CODE GENERATION
// Machine code emission for 4D array operations

#include "blaze_internals.h"

// Forward declarations
extern void emit_mov_reg_imm64(CodeBuffer* buf, X64Register reg, uint64_t value);
extern void emit_mov_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_add_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_mul_reg(CodeBuffer* buf, X64Register reg);
extern void emit_push_reg(CodeBuffer* buf, X64Register reg);
extern void emit_pop_reg(CodeBuffer* buf, X64Register reg);
extern void emit_mov_mem_reg(CodeBuffer* buf, X64Register base, int32_t offset, X64Register src);
extern void emit_mov_reg_mem(CodeBuffer* buf, X64Register dst, X64Register base, int32_t offset);
extern void emit_call_reg(CodeBuffer* buf, X64Register reg);
extern void emit_byte(CodeBuffer* buf, uint8_t byte);

// Array4D functions from runtime
extern Array4D* array4d_create(uint32_t x, uint32_t y, uint32_t z, uint32_t t, size_t elem_size);
extern void array4d_set(Array4D* arr, int x, int y, int z, int t, void* value);
extern bool array4d_get(Array4D* arr, int x, int y, int z, int t, void* out_value);

// Generate code to create 4D array
void generate_array4d_create(CodeBuffer* buf, ASTNode* nodes, uint16_t node_idx, 
                            SymbolTable* symbols, char* string_pool) {
    if (nodes[node_idx].type != NODE_ARRAY_4D_DEF) return;
    
    // Get array name
    uint16_t name_idx = nodes[node_idx].data.array_4d.name_idx;
    ASTNode* name_node = &nodes[name_idx];
    const char* array_name = &string_pool[name_node->data.ident.name_offset];
    
    // Evaluate dimension expressions
    // For now, assume they're constants
    uint32_t dimensions[4];
    for (int i = 0; i < 4; i++) {
        uint16_t dim_idx = nodes[node_idx].data.array_4d.dim_indices[i];
        if (nodes[dim_idx].type == NODE_NUMBER) {
            dimensions[i] = (uint32_t)nodes[dim_idx].data.number;
        } else {
            dimensions[i] = 10; // Default size
        }
    }
    
    // Calculate total size needed for array
    // Size = dimensions[0] * dimensions[1] * dimensions[2] * dimensions[3] * elem_size + header
    uint64_t total_elements = dimensions[0] * dimensions[1] * dimensions[2] * dimensions[3];
    uint64_t elem_size = 8; // 8-byte elements (doubles)
    uint64_t data_size = total_elements * elem_size;
    uint64_t header_size = 64; // Array4D header size (estimated)
    uint64_t total_size = header_size + data_size;
    
    // Use rc_alloc for persistent arrays (they need to survive action blocks)
    extern void generate_rc_alloc(CodeBuffer* buf, X64Register size_reg, X64Register result_reg);
    emit_mov_reg_imm64(buf, RDI, total_size);
    generate_rc_alloc(buf, RDI, RAX);
    
    // Now initialize the array structure
    // RAX contains the allocated memory pointer
    // Initialize array header with dimensions
    emit_mov_mem_reg(buf, RAX, 0, RDI);  // Store x dimension
    emit_mov_reg_imm64(buf, RDI, dimensions[0]);
    emit_mov_mem_reg(buf, RAX, 0, RDI);
    
    emit_mov_reg_imm64(buf, RDI, dimensions[1]);
    emit_mov_mem_reg(buf, RAX, 8, RDI);  // Store y dimension
    
    emit_mov_reg_imm64(buf, RDI, dimensions[2]);
    emit_mov_mem_reg(buf, RAX, 16, RDI); // Store z dimension
    
    emit_mov_reg_imm64(buf, RDI, dimensions[3]);
    emit_mov_mem_reg(buf, RAX, 24, RDI); // Store t dimension
    
    emit_mov_reg_imm64(buf, RDI, elem_size);
    emit_mov_mem_reg(buf, RAX, 32, RDI); // Store element size
    
    // Store array pointer in symbol table
    Symbol* array_sym = symbol_add_array_4d(symbols, array_name, 
                                           dimensions[0], dimensions[1], dimensions[2], dimensions[3]);
    if (array_sym) {
        // Save array pointer to stack location
        emit_mov_mem_reg(buf, RBP, array_sym->data.array_4d.base_addr, RAX);
    }
}

// Generate code for array access
void generate_array4d_access(CodeBuffer* buf, ASTNode* nodes, uint16_t node_idx,
                           SymbolTable* symbols, char* string_pool, bool is_lvalue) {
    if (nodes[node_idx].type != NODE_ARRAY_4D_ACCESS) return;
    
    // Get array name
    uint16_t name_idx = nodes[node_idx].data.array_4d.name_idx;
    ASTNode* name_node = &nodes[name_idx];
    const char* array_name = &string_pool[name_node->data.ident.name_offset];
    
    // Look up array in symbol table
    Symbol* array_sym = symbol_lookup(symbols, array_name, name_node->data.ident.name_len, false);
    if (!array_sym || array_sym->type != SYMBOL_ARRAY_4D) return;
    
    // Load array pointer
    emit_mov_reg_mem(buf, R15, RBP, array_sym->data.array_4d.base_addr);
    
    // Evaluate indices and push them
    for (int i = 0; i < 4; i++) {
        uint16_t idx_node = nodes[node_idx].data.array_4d.dim_indices[i];
        
        if (nodes[idx_node].type == NODE_NUMBER) {
            // Constant index
            emit_mov_reg_imm64(buf, RAX, nodes[idx_node].data.number);
        } else if (nodes[idx_node].type == NODE_TIMING_OP) {
            // Temporal index like <t or >t
            // For now, just use current time
            emit_mov_reg_imm64(buf, RAX, 0);
            
            // Adjust based on timing operator
            if (nodes[idx_node].data.timing.timing_op == TOK_LT) {
                // Past: current_time - offset
                emit_mov_reg_imm64(buf, RCX, 1);
                emit_byte(buf, 0x48); emit_byte(buf, 0x29); emit_byte(buf, 0xC8); // SUB RAX, RCX
            } else if (nodes[idx_node].data.timing.timing_op == TOK_GT) {
                // Future: current_time + offset
                emit_mov_reg_imm64(buf, RCX, 1);
                emit_add_reg_reg(buf, RAX, RCX);
            }
        } else {
            // Expression - evaluate it
            // (Would call expression generator here)
            emit_mov_reg_imm64(buf, RAX, 0);
        }
        
        // Push index
        emit_push_reg(buf, RAX);
    }
    
    // Pop indices in reverse order
    emit_pop_reg(buf, R11); // t
    emit_pop_reg(buf, R10); // z
    emit_pop_reg(buf, R9);  // y
    emit_pop_reg(buf, R8);  // x
    
    if (is_lvalue) {
        // For assignment - return address calculation in RAX
        // Calculate: base + x*stride[0] + y*stride[1] + z*stride[2] + t*stride[3]
        
        // x * elem_size
        emit_mov_reg_reg(buf, RAX, R8);
        emit_mov_reg_imm64(buf, RCX, 8); // elem_size
        emit_mul_reg(buf, RCX);
        
        // Save result
        emit_mov_reg_reg(buf, RDI, RAX);
        
        // y * (x_dim * elem_size)
        emit_mov_reg_reg(buf, RAX, R9);
        emit_mov_reg_imm64(buf, RCX, array_sym->data.array_4d.dimensions[0] * 8);
        emit_mul_reg(buf, RCX);
        emit_add_reg_reg(buf, RDI, RAX);
        
        // z * (x_dim * y_dim * elem_size)
        emit_mov_reg_reg(buf, RAX, R10);
        emit_mov_reg_imm64(buf, RCX, array_sym->data.array_4d.dimensions[0] * 
                                    array_sym->data.array_4d.dimensions[1] * 8);
        emit_mul_reg(buf, RCX);
        emit_add_reg_reg(buf, RDI, RAX);
        
        // t * (x_dim * y_dim * z_dim * elem_size)
        emit_mov_reg_reg(buf, RAX, R11);
        emit_mov_reg_imm64(buf, RCX, array_sym->data.array_4d.dimensions[0] * 
                                    array_sym->data.array_4d.dimensions[1] * 
                                    array_sym->data.array_4d.dimensions[2] * 8);
        emit_mul_reg(buf, RCX);
        emit_add_reg_reg(buf, RDI, RAX);
        
        // Add base pointer
        emit_add_reg_reg(buf, RDI, R15);
        
        // Return address in RAX
        emit_mov_reg_reg(buf, RAX, RDI);
        
    } else {
        // For read - call array4d_get
        // Arguments: RDI=arr, RSI=x, RDX=y, RCX=z, R8=t, R9=&out_value
        emit_mov_reg_reg(buf, RDI, R15);    // array pointer
        emit_mov_reg_reg(buf, RSI, R8);     // x
        emit_mov_reg_reg(buf, RDX, R9);     // y
        emit_mov_reg_reg(buf, RCX, R10);    // z
        emit_mov_reg_reg(buf, R8, R11);     // t
        
        // Allocate space for output
        emit_byte(buf, 0x48); emit_byte(buf, 0x83); emit_byte(buf, 0xEC); emit_byte(buf, 0x08); // SUB RSP, 8
        emit_mov_reg_reg(buf, R9, RSP);     // &out_value
        
        // Call array4d_get
        emit_mov_reg_imm64(buf, RAX, (uint64_t)array4d_get);
        emit_byte(buf, 0xFF);
        emit_byte(buf, 0xD0); // CALL RAX
        
        // Load result
        emit_mov_reg_mem(buf, RAX, RSP, 0);
        
        // Clean up stack
        emit_byte(buf, 0x48); emit_byte(buf, 0x83); emit_byte(buf, 0xC4); emit_byte(buf, 0x08); // ADD RSP, 8
    }
}

// Generate assignment to 4D array element
void generate_array4d_assign(CodeBuffer* buf, ASTNode* nodes, uint16_t lhs_idx, 
                           uint16_t rhs_idx, SymbolTable* symbols, char* string_pool) {
    // Evaluate RHS first
    // (Would call expression generator here)
    if (nodes[rhs_idx].type == NODE_NUMBER) {
        emit_mov_reg_imm64(buf, R14, nodes[rhs_idx].data.number);
    } else {
        emit_mov_reg_imm64(buf, R14, 42); // Default value
    }
    
    // Get array element address
    generate_array4d_access(buf, nodes, lhs_idx, symbols, string_pool, true);
    
    // Store value
    emit_mov_mem_reg(buf, RAX, 0, R14);
}

// emit_call_reg is defined in codegen_x64.c