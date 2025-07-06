// Basic code generation for core Blaze operations

#include "blaze_internals.h"

// Define int8_t if not available
typedef signed char int8_t;

// Forward declarations
extern void emit_mov_reg_imm64(CodeBuffer* buf, X64Register reg, uint64_t value);
extern void emit_mov_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_mov_mem_reg(CodeBuffer* buf, X64Register base, int32_t offset, X64Register src);
extern void emit_mov_reg_mem(CodeBuffer* buf, X64Register dst, X64Register base, int32_t offset);
extern void emit_syscall(CodeBuffer* buf);
extern void emit_byte(CodeBuffer* buf, uint8_t byte);
extern void emit_push_reg(CodeBuffer* buf, X64Register reg);
extern void emit_pop_reg(CodeBuffer* buf, X64Register reg);
extern void emit_lea(CodeBuffer* buf, X64Register dst, X64Register base, int32_t offset);
extern void emit_add_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_sub_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_sub_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_cmp_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);

// Forward declaration for new variable system
void generate_var_def_new(CodeBuffer* buf, ASTNode* nodes, uint16_t node_idx, 
                          SymbolTable* symbols, char* string_pool);
extern void generate_identifier(CodeBuffer* buf, ASTNode* nodes, uint16_t node_idx,
                                SymbolTable* symbols, char* string_pool);

// SSE register definitions


// Forward declarations for SSE instructions
extern void emit_movsd_xmm_imm(CodeBuffer* buf, SSERegister reg, double value);
extern void emit_movsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src);
extern void emit_movsd_xmm_mem(CodeBuffer* buf, SSERegister dst, X64Register base);
extern void emit_movsd_mem_xmm(CodeBuffer* buf, X64Register base, SSERegister src);
extern void emit_addsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src);
extern void emit_subsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src);
extern void emit_mulsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src);
extern void emit_divsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src);
extern void emit_cvtsi2sd_xmm_reg(CodeBuffer* buf, SSERegister dst, X64Register src);
extern void emit_cvtsd2si_reg_xmm(CodeBuffer* buf, X64Register dst, SSERegister src);

// Forward declarations for function system
extern void generate_func_def(CodeBuffer* buf, ASTNode* nodes, uint16_t func_idx, 
                             SymbolTable* symbols, char* string_pool);
extern void generate_func_call(CodeBuffer* buf, ASTNode* nodes, uint16_t call_idx,
                              SymbolTable* symbols, char* string_pool);

extern void generate_conditional(CodeBuffer* buf, ASTNode* nodes, uint16_t cond_idx,
                               SymbolTable* symbols, char* string_pool);

// Forward declaration for float printing
extern void generate_print_float(CodeBuffer* buf);
extern void generate_print_float_safe(CodeBuffer* buf);

// Variable type definitions from codegen_vars.c
#define VAR_TYPE_INT    0
#define VAR_TYPE_FLOAT  1
#define VAR_TYPE_SOLID  4
#define VAR_TYPE_STRING 2
#define VAR_TYPE_BOOL   3

typedef struct {
    uint32_t name_hash;
    int32_t stack_offset;
    bool is_initialized;
    uint8_t var_type;
} VarEntry;

extern VarEntry* get_or_create_var(const char* name);

// Generate code for variable definition
void generate_var_def(CodeBuffer* buf, ASTNode* nodes, uint16_t node_idx, 
                      SymbolTable* symbols, char* string_pool) {
    // Use the new variable system
    generate_var_def_new(buf, nodes, node_idx, symbols, string_pool);
}

// Generate code for print operation
void generate_print(CodeBuffer* buf, const char* message, uint32_t len) {
    // For now, embed the string directly in the code section
    // This works for all platforms
    
    // Jump over the string data
    emit_byte(buf, 0xEB);  // JMP short
    emit_byte(buf, len);   // Skip len bytes
    
    // Remember where the string starts
    uint32_t string_pos = buf->position;
    
    // Embed the string bytes
    for (uint32_t i = 0; i < len; i++) {
        emit_byte(buf, message[i]);
    }
    
    // Now generate platform-specific code to print the string
    // The string is at [RIP - (current_pos - string_pos)]
    
    if (buf->target_platform == PLATFORM_LINUX) {
        // lea rsi, [rip - offset]
        emit_byte(buf, 0x48);  // REX.W
        emit_byte(buf, 0x8D);  // LEA
        emit_byte(buf, 0x35);  // ModRM for RSI, [RIP+disp32]
        
        // Calculate offset: string_pos - (current_pos + 4)
        int32_t offset = string_pos - (buf->position + 4);
        emit_byte(buf, offset & 0xFF);
        emit_byte(buf, (offset >> 8) & 0xFF);
        emit_byte(buf, (offset >> 16) & 0xFF);
        emit_byte(buf, (offset >> 24) & 0xFF);
        
        // mov rax, 1 (sys_write)
        emit_mov_reg_imm64(buf, RAX, 1);
        
        // mov rdi, 1 (stdout)
        emit_mov_reg_imm64(buf, RDI, 1);
        
        // mov rdx, len
        emit_mov_reg_imm64(buf, RDX, len);
        
        // syscall
        emit_syscall(buf);
    } else if (buf->target_platform == PLATFORM_WINDOWS) {
        // Windows: Use imported WriteConsoleA function
        // The IAT is at RVA 0x2060 (idata_rva + 0x60)
        // GetStdHandle is at [0x140002060]
        // WriteConsoleA is at [0x140002068]
        
        // Preserve R12 (we'll use it to store the console handle)
        emit_push_reg(buf, R12);
        
        // First, get console handle using GetStdHandle
        // mov rcx, -11 (STD_OUTPUT_HANDLE)
        emit_mov_reg_imm64(buf, RCX, 0xFFFFFFFFFFFFFFF5);
        
        // Allocate shadow space
        emit_sub_reg_imm32(buf, RSP, 0x28);
        
        // Call GetStdHandle via IAT
        // We need to use RIP-relative addressing to access the IAT
        // The IAT is at virtual address 0x2060, and we're executing from 0x1000
        // So we need to calculate the offset from current RIP
        // mov rax, [rip + offset]
        emit_byte(buf, 0x48); // REX.W
        emit_byte(buf, 0x8B); // MOV
        emit_byte(buf, 0x05); // ModRM for RAX, [RIP+disp32]
        
        // Calculate offset from current position to IAT entry
        // Assuming we're at around 0x1000 + current position
        // and IAT is at 0x2060
        int32_t iat_offset = 0x2060 - (0x1000 + buf->position + 4);
        emit_byte(buf, iat_offset & 0xFF);
        emit_byte(buf, (iat_offset >> 8) & 0xFF);
        emit_byte(buf, (iat_offset >> 16) & 0xFF);
        emit_byte(buf, (iat_offset >> 24) & 0xFF);
        
        // call rax
        emit_byte(buf, 0xFF);
        emit_byte(buf, 0xD0);
        
        // Save handle in R12 (preserved across calls)
        emit_mov_reg_reg(buf, R12, RAX);
        
        // Clean up shadow space
        emit_add_reg_imm32(buf, RSP, 0x28);
        
        // Now call WriteConsoleA
        // lea rdx, [rip - offset] ; Buffer
        emit_byte(buf, 0x48);  // REX.W
        emit_byte(buf, 0x8D);  // LEA
        emit_byte(buf, 0x15);  // ModRM for RDX, [RIP+disp32]
        
        // Calculate offset: string_pos - (current_pos + 4)
        int32_t offset = string_pos - (buf->position + 4);
        emit_byte(buf, offset & 0xFF);
        emit_byte(buf, (offset >> 8) & 0xFF);
        emit_byte(buf, (offset >> 16) & 0xFF);
        emit_byte(buf, (offset >> 24) & 0xFF);
        
        // Set up parameters for WriteConsoleA
        // RCX = hConsoleOutput (from R12)
        emit_mov_reg_reg(buf, RCX, R12);
        
        // RDX = lpBuffer (already set by LEA above)
        
        // R8 = nNumberOfCharsToWrite
        emit_mov_reg_imm64(buf, R8, len);
        
        // R9 = lpNumberOfCharsWritten (NULL)
        emit_xor_reg_reg(buf, R9, R9);
        
        // [RSP+0x20] = lpReserved (NULL) - 5th parameter
        // Allocate shadow space + space for 5th parameter
        emit_sub_reg_imm32(buf, RSP, 0x28);
        
        // mov qword [rsp+0x20], 0
        emit_byte(buf, 0x48); // REX.W
        emit_byte(buf, 0xC7); // MOV
        emit_byte(buf, 0x44); // ModRM
        emit_byte(buf, 0x24); // SIB
        emit_byte(buf, 0x20); // disp8
        emit_byte(buf, 0x00); // imm32 (0)
        emit_byte(buf, 0x00);
        emit_byte(buf, 0x00);
        emit_byte(buf, 0x00);
        
        // Call WriteConsoleA via IAT
        // mov rax, [rip + offset]
        emit_byte(buf, 0x48); // REX.W
        emit_byte(buf, 0x8B); // MOV
        emit_byte(buf, 0x05); // ModRM for RAX, [RIP+disp32]
        
        // WriteConsoleA is at IAT offset 0x2068
        iat_offset = 0x2068 - (0x1000 + buf->position + 4);
        emit_byte(buf, iat_offset & 0xFF);
        emit_byte(buf, (iat_offset >> 8) & 0xFF);
        emit_byte(buf, (iat_offset >> 16) & 0xFF);
        emit_byte(buf, (iat_offset >> 24) & 0xFF);
        
        // call rax
        emit_byte(buf, 0xFF);
        emit_byte(buf, 0xD0);
        
        // Clean up stack
        emit_add_reg_imm32(buf, RSP, 0x28);
        
        // Restore R12
        emit_pop_reg(buf, R12);
    }
}

// Forward declarations for multi-digit support
extern void emit_xor_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_div_reg(CodeBuffer* buf, X64Register divisor);
extern void emit_inc_reg(CodeBuffer* buf, X64Register reg);
extern void emit_test_reg_reg(CodeBuffer* buf, X64Register reg1, X64Register reg2);
extern void emit_jz(CodeBuffer* buf, int8_t offset);
extern void emit_jnz(CodeBuffer* buf, int8_t offset);
extern void emit_shl_reg_imm8(CodeBuffer* buf, X64Register reg, uint8_t count);
extern void emit_dec_reg(CodeBuffer* buf, X64Register reg);

// Generate code to print a number from register
void generate_print_number(CodeBuffer* buf, X64Register num_reg) {
    // Save all registers we'll use
    emit_push_reg(buf, RCX);
    emit_push_reg(buf, RDX);
    emit_push_reg(buf, RBX);
    emit_push_reg(buf, RSI);
    emit_push_reg(buf, RDI);
    
    // Move value to RAX if it's not already there
    if (num_reg != RAX) {
        emit_push_reg(buf, RAX);
        emit_mov_reg_reg(buf, RAX, num_reg);
    }
    
    // Special case for 0
    emit_test_reg_reg(buf, RAX, RAX);
    uint32_t not_zero_jump = buf->position;
    emit_jnz(buf, 0); // placeholder
    
    // It's zero, print '0'
    // Allocate space for character on stack
    emit_sub_reg_imm32(buf, RSP, 8);
    emit_mov_reg_imm64(buf, RAX, '0');
    emit_mov_mem_reg(buf, RSP, 0, RAX);  // Store '0' at [RSP]
    
    // Use platform-aware character output
    extern void emit_platform_print_char(CodeBuffer* buf, Platform platform);
    emit_platform_print_char(buf, buf->target_platform);
    
    emit_add_reg_imm32(buf, RSP, 8); // Clean up stack
    
    // Jump to end
    uint32_t to_end_from_zero = buf->position;
    emit_byte(buf, 0xE9); // jmp near
    emit_byte(buf, 0x00); emit_byte(buf, 0x00); emit_byte(buf, 0x00); emit_byte(buf, 0x00); // placeholder
    
    // Patch not_zero jump
    uint8_t* patch_not_zero = &buf->code[not_zero_jump + 1];
    *patch_not_zero = buf->position - not_zero_jump - 2;
    
    // Count digits and build string backwards
    emit_xor_reg_reg(buf, RCX, RCX); // RCX = digit count
    emit_mov_reg_imm64(buf, RBX, 10); // divisor
    
    // Digit extraction loop
    uint32_t digit_loop_start = buf->position;
    
    // Clear RDX for division
    emit_xor_reg_reg(buf, RDX, RDX);
    
    // Divide RAX by 10, quotient in RAX, remainder in RDX
    emit_div_reg(buf, RBX);
    
    // Convert remainder to ASCII and store on stack
    emit_add_reg_imm32(buf, RDX, '0');
    emit_sub_reg_imm32(buf, RSP, 8);  // Make space
    emit_mov_mem_reg(buf, RSP, 0, RDX);  // Store digit at [RSP]
    emit_inc_reg(buf, RCX); // increment digit count
    
    // Check if quotient is 0
    emit_test_reg_reg(buf, RAX, RAX);
    int8_t loop_offset = digit_loop_start - (buf->position + 2);
    emit_jnz(buf, loop_offset); // jump back to loop if not zero
    
    // Now print the digits
    // RCX has the count, stack has digits in reverse order
    emit_mov_reg_reg(buf, RBX, RCX); // save count
    
    uint32_t print_loop_start = buf->position;
    emit_test_reg_reg(buf, RBX, RBX);
    uint32_t print_done_jump = buf->position;
    emit_jz(buf, 0); // placeholder
    
    // Print one digit
    extern void emit_platform_print_char(CodeBuffer* buf, Platform platform);
    emit_platform_print_char(buf, buf->target_platform);
    
    emit_add_reg_imm32(buf, RSP, 8); // remove printed digit
    emit_sub_reg_imm32(buf, RBX, 1); // decrement count
    
    // Jump back to print loop
    int8_t print_loop_offset = print_loop_start - (buf->position + 2);
    emit_byte(buf, 0xEB); // jmp short
    emit_byte(buf, print_loop_offset);
    
    // Patch print done jump
    uint8_t* patch_print_done = &buf->code[print_done_jump + 1];
    *patch_print_done = buf->position - print_done_jump - 2;
    
    // Patch jump from zero case
    uint32_t* patch_zero_end = (uint32_t*)&buf->code[to_end_from_zero + 1];
    *patch_zero_end = buf->position - to_end_from_zero - 5;
    
    // Print newline
    emit_sub_reg_imm32(buf, RSP, 8);
    emit_mov_reg_imm64(buf, RAX, '\n');
    emit_mov_mem_reg(buf, RSP, 0, RAX);  // Store newline at [RSP]
    
    // Use platform-aware character output
    emit_platform_print_char(buf, buf->target_platform);
    
    emit_add_reg_imm32(buf, RSP, 8); // Clean up stack
    
    // Restore registers
    emit_pop_reg(buf, RDI);
    emit_pop_reg(buf, RSI);
    emit_pop_reg(buf, RBX);
    emit_pop_reg(buf, RDX);
    emit_pop_reg(buf, RCX);
    
    // Only restore RAX if we saved it
    if (num_reg != RAX) {
        emit_pop_reg(buf, RAX);
    }
}

// Variable type definitions (matching codegen_vars.c)
#define VAR_TYPE_FLOAT  1
#define VAR_TYPE_SOLID  4

// Forward declarations for variable functions
extern bool is_var_float(const char* name);

// Check if expression produces a float value
bool is_float_expression_impl(ASTNode* nodes, uint16_t expr_idx, char* string_pool) {
    if (expr_idx == 0 || expr_idx >= 4096) return false;
    
    ASTNode* expr = &nodes[expr_idx];
    
    print_str("[FLOAT_CHECK] Checking node ");
    print_num(expr_idx);
    print_str(" type=");
    print_num(expr->type);
    print_str("\n");
    
    switch (expr->type) {
        case NODE_FLOAT:
            return true;
            
        case NODE_EXPRESSION:
            // Expression nodes use binary structure
            // Check the left side (the actual expression)
            if (expr->data.binary.left_idx > 0 && expr->data.binary.left_idx < 4096) {
                return is_float_expression_impl(nodes, expr->data.binary.left_idx, string_pool);
            }
            return false;
            
        case NODE_BINARY_OP: {
            // If either operand is float, the result is float
            uint16_t left_idx = expr->data.binary.left_idx;
            uint16_t right_idx = expr->data.binary.right_idx;
            return is_float_expression_impl(nodes, left_idx, string_pool) || 
                   is_float_expression_impl(nodes, right_idx, string_pool);
        }
        
        case NODE_IDENTIFIER: {
            // Check if variable is a float
            if (!string_pool) {
                print_str("[FLOAT_CHECK] No string_pool, can't check identifier\n");
                return false;
            }
            
            char var_name[256];
            uint32_t name_len = expr->data.ident.name_len;
            if (name_len >= 256) name_len = 255;
            
            // Extract variable name
            for (uint32_t i = 0; i < name_len; i++) {
                var_name[i] = string_pool[expr->data.ident.name_offset + i];
            }
            var_name[name_len] = '\0';
            
            // Check if variable is a float
            bool is_float = is_var_float(var_name);
            
            print_str("[FLOAT_CHECK] Variable '");
            print_str(var_name);
            print_str("' is_float=");
            print_num(is_float);
            print_str("\n");
            
            return is_float;
        }
        
        case NODE_FUNC_CALL: {
            // All math functions return floats
            print_str("[FLOAT_CHECK] NODE_FUNC_CALL - math functions return float\n");
            return true;
        }
        
        default:
            return false;
    }
}

// Wrapper for backwards compatibility
bool is_float_expression(ASTNode* nodes, uint16_t expr_idx) {
    return is_float_expression_impl(nodes, expr_idx, NULL);
}

// Check if expression is a solid number
bool is_solid_expression_impl(ASTNode* nodes, uint16_t expr_idx, char* string_pool) {
    if (expr_idx == 0 || expr_idx >= 4096) return false;
    
    ASTNode* expr = &nodes[expr_idx];
    
    switch (expr->type) {
        case NODE_SOLID:
            return true;
            
        case NODE_EXPRESSION:
            // Check the inner expression
            if (expr->data.binary.left_idx > 0 && expr->data.binary.left_idx < 4096) {
                return is_solid_expression_impl(nodes, expr->data.binary.left_idx, string_pool);
            }
            return false;
            
        case NODE_BINARY_OP: {
            // If either operand is solid, the result is solid
            uint16_t left_idx = expr->data.binary.left_idx;
            uint16_t right_idx = expr->data.binary.right_idx;
            return is_solid_expression_impl(nodes, left_idx, string_pool) || 
                   is_solid_expression_impl(nodes, right_idx, string_pool);
        }
        
        case NODE_IDENTIFIER: {
            // Check if variable is a solid number
            if (!string_pool) return false;
            
            char var_name[256];
            uint32_t name_len = expr->data.ident.name_len;
            if (name_len >= 256) name_len = 255;
            
            // Extract variable name
            for (uint32_t i = 0; i < name_len; i++) {
                var_name[i] = string_pool[expr->data.ident.name_offset + i];
            }
            var_name[name_len] = '\0';
            
            // Check if variable is a solid number
            return is_var_solid(var_name);
        }
        
        default:
            return false;
    }
}

// Wrapper for solid expression check
bool is_solid_expression(ASTNode* nodes, uint16_t expr_idx) {
    return is_solid_expression_impl(nodes, expr_idx, NULL);
}

// Generate code for expression evaluation
// Result is left in RAX for integers or XMM0 for floats
void generate_expression(CodeBuffer* buf, ASTNode* nodes, uint16_t expr_idx,
                        SymbolTable* symbols, char* string_pool) {
    if (expr_idx == 0 || expr_idx >= 4096) return;
    
    ASTNode* expr = &nodes[expr_idx];
    
    switch (expr->type) {
        case NODE_NUMBER: {
            // Load immediate value into RAX
            int64_t value = expr->data.number;
            emit_mov_reg_imm64(buf, RAX, value);
            break;
        }
        
        case NODE_FLOAT: {
            // Load immediate float value into XMM0
            double value = expr->data.float_value;
            print_str("[EXPR] Loading float value ");
            // Print integer part for debugging
            print_num((int)value);
            print_str(".");
            print_num((int)((value - (int)value) * 100));
            print_str(" into XMM0\n");
            emit_movsd_xmm_imm(buf, XMM0, value);
            break;
        }
        
        case NODE_IDENTIFIER: {
            // Load variable value using the variable system
            generate_identifier(buf, nodes, expr_idx, symbols, string_pool);
            // Result is now in RAX
            break;
        }
        
        case NODE_SOLID: {
            // Create solid number from AST node
            print_str("[EXPR] Loading solid number\n");
            
            // Call our solid number code generator
            extern void generate_solid_literal(CodeBuffer* buf, ASTNode* nodes, 
                                             uint16_t node_idx, char* string_pool);
            generate_solid_literal(buf, nodes, expr_idx, string_pool);
            // Result (solid pointer) is now in RAX
            break;
        }
        
        case NODE_FUNC_CALL: {
            // Handle function calls in expressions
            print_str("[EXPR] Generating function call\n");
            generate_func_call(buf, nodes, expr_idx, symbols, string_pool);
            // Result is in RAX (for integers) or XMM0 (for floats)
            break;
        }
        
        case NODE_BINARY_OP: {
            // Evaluate binary operation
            uint16_t left_idx = expr->data.binary.left_idx;
            uint16_t right_idx = expr->data.binary.right_idx;
            TokenType op = expr->data.binary.op;
            
            print_str("[BINARY] Processing binary op type=");
            print_num(op);
            print_str(" left=");
            print_num(left_idx);
            print_str(" right=");
            print_num(right_idx);
            print_str("\n");
            
            // Check if this is a float or solid operation
            // We need to check if either operand is a float or solid
            bool left_is_float = is_float_expression_impl(nodes, left_idx, string_pool);
            bool right_is_float = is_float_expression_impl(nodes, right_idx, string_pool);
            bool left_is_solid = is_solid_expression_impl(nodes, left_idx, string_pool);
            bool right_is_solid = is_solid_expression_impl(nodes, right_idx, string_pool);
            bool is_float = left_is_float || right_is_float;
            bool is_solid = left_is_solid || right_is_solid;
            
            print_str("[BINARY] left_is_float=");
            print_num(left_is_float);
            print_str(" right_is_float=");
            print_num(right_is_float);
            print_str(" is_solid=");
            print_num(is_solid);
            print_str("\n");
            
            if (is_solid) {
                print_str("[BINARY] Performing solid number operation\n");
                // Call our solid arithmetic code generator
                extern void generate_solid_arithmetic(CodeBuffer* buf, ASTNode* nodes, 
                                                    uint16_t left_idx, uint16_t right_idx,
                                                    TokenType op, SymbolTable* symbols, 
                                                    char* string_pool);
                generate_solid_arithmetic(buf, nodes, left_idx, right_idx, op, 
                                        symbols, string_pool);
                // Result (solid pointer) is now in RAX
            } else if (is_float) {
                print_str("[BINARY] Performing float operation\n");
                // Float operation using SSE
                // Evaluate right operand first
                generate_expression(buf, nodes, right_idx, symbols, string_pool);
                
                if (right_is_float) {
                    // Result is in XMM0, save to stack
                    print_str("[BINARY] Saving right operand (float) to stack\n");
                    emit_sub_reg_imm32(buf, RSP, 8);
                    emit_movsd_mem_xmm(buf, RSP, XMM0);
                } else {
                    // Result is in RAX, convert to float and save
                    emit_cvtsi2sd_xmm_reg(buf, XMM0, RAX);
                    emit_sub_reg_imm32(buf, RSP, 8);
                    emit_movsd_mem_xmm(buf, RSP, XMM0);
                }
                
                // Evaluate left operand
                generate_expression(buf, nodes, left_idx, symbols, string_pool);
                
                if (!left_is_float) {
                    // Convert integer result to float
                    emit_cvtsi2sd_xmm_reg(buf, XMM0, RAX);
                }
                // Left result is now in XMM0
                
                // Load right operand from stack into XMM1
                print_str("[BINARY] Loading saved right operand from stack to XMM1\n");
                emit_movsd_xmm_mem(buf, XMM1, RSP);
                emit_add_reg_imm32(buf, RSP, 8);
                
                // Perform float operation (result in XMM0)
                switch (op) {
                    case TOK_PLUS:
                        emit_addsd_xmm_xmm(buf, XMM0, XMM1);
                        break;
                    case TOK_MINUS:
                        emit_subsd_xmm_xmm(buf, XMM0, XMM1);
                        break;
                    case TOK_STAR:
                        print_str("[BINARY] Emitting mulsd xmm0, xmm1\n");
                        emit_mulsd_xmm_xmm(buf, XMM0, XMM1);
                        break;
                    case TOK_DIV:
                        emit_divsd_xmm_xmm(buf, XMM0, XMM1);
                        break;
                    default:
                        // Unsupported float operation
                        break;
                }
            } else {
                // Integer operation (existing code)
                // Evaluate right operand first
                generate_expression(buf, nodes, right_idx, symbols, string_pool);
                // Save right operand in R10 (a register that won't be clobbered)
                emit_mov_reg_reg(buf, R10, RAX);
                
                // Evaluate left operand
                generate_expression(buf, nodes, left_idx, symbols, string_pool);
                // Now RAX has left operand
                
                // Move right operand to RDX
                emit_mov_reg_reg(buf, RDX, R10);
                
                // Perform operation (result in RAX)
                switch (op) {
                case TOK_PLUS:
                    // Check if right operand is a constant for optimization
                    if (right_idx < 4096 && nodes[right_idx].type == NODE_NUMBER) {
                        int64_t constant = nodes[right_idx].data.number;
                        
                        if (constant == 1) {
                            // x + 1 = inc x
                            print_str("[OPT] Using INC for +1\n");
                            emit_inc_reg(buf, RAX);
                        }
                        else if (constant == -1) {
                            // x + (-1) = dec x
                            emit_dec_reg(buf, RAX);
                        }
                        else if (constant >= -2147483648LL && constant <= 2147483647LL) {
                            // Use LEA for small constants
                            // lea rax, [rax + constant]
                            emit_lea(buf, RAX, RAX, (int32_t)constant);
                        }
                        else {
                            // Regular addition
                            emit_add_reg_reg(buf, RAX, RDX);
                        }
                    } else {
                        // Regular addition
                        emit_add_reg_reg(buf, RAX, RDX);
                    }
                    break;
                    
                case TOK_MINUS:
                    emit_sub_reg_reg(buf, RAX, RDX);
                    break;
                    
                case TOK_STAR:
                    // Check if right operand is a constant for optimization
                    if (right_idx < 4096 && nodes[right_idx].type == NODE_NUMBER) {
                        int64_t constant = nodes[right_idx].data.number;
                        
                        // Power of 2 optimization
                        if (constant > 0 && (constant & (constant - 1)) == 0) {
                            // Find shift amount
                            uint8_t shift = 0;
                            int64_t temp = constant;
                            while (temp > 1) {
                                temp >>= 1;
                                shift++;
                            }
                            // Left operand is already in RAX
                            print_str("[OPT] Using SHL for *");
                            print_num(constant);
                            print_str("\n");
                            emit_shl_reg_imm8(buf, RAX, shift);
                        }
                        // LEA optimizations for small multipliers
                        else if (constant == 3) {
                            // lea rax, [rax + rax*2]
                            emit_byte(buf, 0x48); // REX.W
                            emit_byte(buf, 0x8D);
                            emit_byte(buf, 0x04);
                            emit_byte(buf, 0x40);
                        }
                        else if (constant == 5) {
                            // lea rax, [rax + rax*4]
                            emit_byte(buf, 0x48); // REX.W
                            emit_byte(buf, 0x8D);
                            emit_byte(buf, 0x04);
                            emit_byte(buf, 0x80);
                        }
                        else if (constant == 9) {
                            // lea rax, [rax + rax*8]
                            emit_byte(buf, 0x48); // REX.W
                            emit_byte(buf, 0x8D);
                            emit_byte(buf, 0x04);
                            emit_byte(buf, 0xC0);
                        }
                        else {
                            // Regular multiplication
                            emit_mul_reg(buf, RDX);
                        }
                    } else {
                        // Regular multiplication
                        emit_mul_reg(buf, RDX);
                    }
                    break;
                    
                case TOK_DIV:
                    // DIV uses RDX:RAX as dividend
                    // RAX = left operand (dividend)
                    // RDX = right operand (divisor)
                    emit_mov_reg_reg(buf, RCX, RDX); // Save divisor in RCX
                    // Clear RDX first (sign extend RAX to RDX:RAX)
                    emit_byte(buf, 0x48); // REX.W
                    emit_byte(buf, 0x99); // CQO
                    // Now divide by RCX
                    emit_div_reg(buf, RCX);
                    // Result is in RAX
                    break;
                    
                case TOK_PERCENT:
                    // Similar to DIV but remainder is in RDX
                    emit_mov_reg_reg(buf, RCX, RDX); // Save divisor in RCX
                    emit_byte(buf, 0x48); // REX.W
                    emit_byte(buf, 0x99); // CQO
                    emit_div_reg(buf, RCX);
                    emit_mov_reg_reg(buf, RAX, RDX); // Move remainder to RAX
                    break;
                    
                case TOK_EXPONENT: {
                    // Exponentiation: base in RAX, exponent in RDX
                    // After binary op evaluation: left (base) in RAX, right (exponent) in RDX
                    
                    // Check for zero exponent first
                    emit_test_reg_reg(buf, RDX, RDX);
                    uint32_t zero_exp_jump = buf->position;
                    emit_jnz(buf, 0); // placeholder - will patch
                    
                    // Exponent is 0, result is 1
                    emit_mov_reg_imm64(buf, RAX, 1);
                    uint32_t done_jump = buf->position;
                    emit_byte(buf, 0xEB); // JMP to end
                    emit_byte(buf, 0); // placeholder
                    
                    // Patch the zero check jump
                    buf->code[zero_exp_jump + 1] = buf->position - zero_exp_jump - 2;
                    
                    // Non-zero exponent: do the loop
                    // Save base in RCX for multiplication
                    emit_mov_reg_reg(buf, RCX, RAX); // RCX = base
                    
                    // Need to save exponent somewhere else since MUL will overwrite RDX
                    emit_mov_reg_reg(buf, RBX, RDX); // Move exponent to RBX
                    
                    emit_mov_reg_imm64(buf, RAX, 1); // RAX = 1 (result)
                    
                    // Loop: while RBX > 0
                    uint32_t loop_start = buf->position;
                    
                    // Clear RDX for multiplication
                    emit_xor_reg_reg(buf, RDX, RDX);
                    
                    // Result *= base
                    emit_mul_reg(buf, RCX); // RDX:RAX = RAX * RCX
                    
                    // Exponent--
                    emit_dec_reg(buf, RBX);
                    
                    // Test if we should continue
                    emit_test_reg_reg(buf, RBX, RBX);
                    int8_t loop_offset = loop_start - (buf->position + 2);
                    emit_jnz(buf, loop_offset); // Jump back if not zero
                    
                    // Patch the done jump
                    buf->code[done_jump + 1] = buf->position - done_jump - 2;
                    
                    // Loop end - result is in RAX
                    break;
                }
                    
                // Comparison operators - set flags and use SETcc
                case TOK_LT_CMP:
                    emit_cmp_reg_reg(buf, RAX, RDX);
                    emit_byte(buf, 0x0F); // SETL
                    emit_byte(buf, 0x9C);
                    emit_byte(buf, 0xC0); // AL
                    emit_byte(buf, 0x48); // MOVZX RAX, AL
                    emit_byte(buf, 0x0F);
                    emit_byte(buf, 0xB6);
                    emit_byte(buf, 0xC0);
                    break;
                    
                case TOK_GT_CMP:
                    emit_cmp_reg_reg(buf, RAX, RDX);
                    emit_byte(buf, 0x0F); // SETG
                    emit_byte(buf, 0x9F);
                    emit_byte(buf, 0xC0); // AL
                    emit_byte(buf, 0x48); // MOVZX RAX, AL
                    emit_byte(buf, 0x0F);
                    emit_byte(buf, 0xB6);
                    emit_byte(buf, 0xC0);
                    break;
                    
                case TOK_EQ:
                    emit_cmp_reg_reg(buf, RAX, RDX);
                    emit_byte(buf, 0x0F); // SETE
                    emit_byte(buf, 0x94);
                    emit_byte(buf, 0xC0); // AL
                    emit_byte(buf, 0x48); // MOVZX RAX, AL
                    emit_byte(buf, 0x0F);
                    emit_byte(buf, 0xB6);
                    emit_byte(buf, 0xC0);
                    break;
                    
                case TOK_NE:
                    emit_cmp_reg_reg(buf, RAX, RDX);
                    emit_byte(buf, 0x0F); // SETNE
                    emit_byte(buf, 0x95);
                    emit_byte(buf, 0xC0); // AL
                    emit_byte(buf, 0x48); // MOVZX RAX, AL
                    emit_byte(buf, 0x0F);
                    emit_byte(buf, 0xB6);
                    emit_byte(buf, 0xC0);
                    break;
                    
                case TOK_LE:
                    emit_cmp_reg_reg(buf, RAX, RDX);
                    emit_byte(buf, 0x0F); // SETLE
                    emit_byte(buf, 0x9E);
                    emit_byte(buf, 0xC0); // AL
                    emit_byte(buf, 0x48); // MOVZX RAX, AL
                    emit_byte(buf, 0x0F);
                    emit_byte(buf, 0xB6);
                    emit_byte(buf, 0xC0);
                    break;
                    
                case TOK_GE:
                    emit_cmp_reg_reg(buf, RAX, RDX);
                    emit_byte(buf, 0x0F); // SETGE  
                    emit_byte(buf, 0x9D);
                    emit_byte(buf, 0xC0); // AL
                    emit_byte(buf, 0x48); // MOVZX RAX, AL
                    emit_byte(buf, 0x0F);
                    emit_byte(buf, 0xB6);
                    emit_byte(buf, 0xC0);
                    break;
                    
                case TOK_AND:
                    // Logical AND: both operands must be non-zero
                    // Test left operand
                    emit_test_reg_reg(buf, RAX, RAX);
                    emit_byte(buf, 0x0F); // SETNZ AL
                    emit_byte(buf, 0x95);
                    emit_byte(buf, 0xC0);
                    emit_byte(buf, 0x0F); // MOVZX RAX, AL
                    emit_byte(buf, 0xB6);
                    emit_byte(buf, 0xC0);
                    
                    // Test right operand
                    emit_test_reg_reg(buf, RDX, RDX);
                    emit_byte(buf, 0x0F); // SETNZ DL
                    emit_byte(buf, 0x95);
                    emit_byte(buf, 0xC2);
                    emit_byte(buf, 0x0F); // MOVZX RDX, DL
                    emit_byte(buf, 0xB6);
                    emit_byte(buf, 0xD2);
                    
                    // AND the results
                    emit_byte(buf, 0x48); // AND RAX, RDX
                    emit_byte(buf, 0x21);
                    emit_byte(buf, 0xD0);
                    break;
                    
                case TOK_OR:
                    // Logical OR: at least one operand must be non-zero
                    // Combine operands
                    emit_byte(buf, 0x48); // OR RAX, RDX
                    emit_byte(buf, 0x09);
                    emit_byte(buf, 0xD0);
                    
                    // Test result and set 0 or 1
                    emit_test_reg_reg(buf, RAX, RAX);
                    emit_byte(buf, 0x0F); // SETNZ AL
                    emit_byte(buf, 0x95);
                    emit_byte(buf, 0xC0);
                    emit_byte(buf, 0x0F); // MOVZX RAX, AL
                    emit_byte(buf, 0xB6);
                    emit_byte(buf, 0xC0);
                    break;
                    
                // Bitwise operators
                case TOK_BIT_AND:
                    // Bitwise AND: RAX & RDX
                    emit_byte(buf, 0x48); // AND RAX, RDX
                    emit_byte(buf, 0x21);
                    emit_byte(buf, 0xD0);
                    break;
                    
                case TOK_BIT_OR:
                    // Bitwise OR: RAX | RDX
                    emit_byte(buf, 0x48); // OR RAX, RDX
                    emit_byte(buf, 0x09);
                    emit_byte(buf, 0xD0);
                    break;
                    
                case TOK_BIT_XOR:
                    // Bitwise XOR: RAX ^ RDX
                    emit_byte(buf, 0x48); // XOR RAX, RDX
                    emit_byte(buf, 0x31);
                    emit_byte(buf, 0xD0);
                    break;
                    
                case TOK_BIT_LSHIFT:
                    // Left shift: RAX << RDX
                    // Move shift count to RCX
                    emit_mov_reg_reg(buf, RCX, RDX);
                    // SHL RAX, CL
                    emit_byte(buf, 0x48);
                    emit_byte(buf, 0xD3);
                    emit_byte(buf, 0xE0);
                    break;
                    
                case TOK_BIT_RSHIFT:
                    // Right shift: RAX >> RDX
                    // Move shift count to RCX
                    emit_mov_reg_reg(buf, RCX, RDX);
                    // SHR RAX, CL
                    emit_byte(buf, 0x48);
                    emit_byte(buf, 0xD3);
                    emit_byte(buf, 0xE8);
                    break;
                    
                default:
                    // Unsupported operation
                    emit_mov_reg_imm64(buf, RAX, 0);
                    break;
                }
            }
            break;
        }
        
        case NODE_UNARY_OP: {
            uint16_t operand_idx = expr->data.unary.expr_idx;
            TokenType op = expr->data.unary.op;
            
            // Generate the expression
            generate_expression(buf, nodes, operand_idx, symbols, string_pool);
            
            // Apply unary operator
            switch (op) {
                case TOK_BANG:
                    // Logical NOT: test if zero
                    emit_test_reg_reg(buf, RAX, RAX);
                    emit_byte(buf, 0x0F); // SETZ AL
                    emit_byte(buf, 0x94);
                    emit_byte(buf, 0xC0);
                    emit_byte(buf, 0x0F); // MOVZX RAX, AL
                    emit_byte(buf, 0xB6);
                    emit_byte(buf, 0xC0);
                    break;
                    
                case TOK_BIT_NOT:
                    // Bitwise NOT: invert all bits
                    emit_byte(buf, 0x48); // NOT RAX
                    emit_byte(buf, 0xF7);
                    emit_byte(buf, 0xD0);
                    break;
                    
                default:
                    // Unsupported unary operator
                    break;
            }
            break;
        }
        
        case NODE_BOOL: {
            // Load boolean value (0 or 1) into RAX
            bool value = expr->data.boolean.value;
            emit_mov_reg_imm64(buf, RAX, value ? 1 : 0);
            break;
        }
        
        default:
            // Unknown expression type
            emit_mov_reg_imm64(buf, RAX, 0);
            break;
    }
}

// Generate code for output node
void generate_output(CodeBuffer* buf, ASTNode* nodes, uint16_t node_idx,
                     SymbolTable* symbols, char* string_pool) {
    if (node_idx == 0 || node_idx >= 4096) {
        print_str("[OUTPUT] Invalid node_idx=");
        print_num(node_idx);
        print_str("\n");
        return;
    }
    
    ASTNode* node = &nodes[node_idx];
    
    // Check node type first before accessing union
    if (node->type != NODE_OUTPUT) {
        print_str("[OUTPUT] Wrong node type=");
        print_num(node->type);
        print_str(" expected NODE_OUTPUT=");
        print_num(NODE_OUTPUT);
        print_str("\n");
        return;
    }
    
    // Debug output
    print_str("[OUTPUT] node_idx=");
    print_num(node_idx);
    print_str(" output_type=");
    print_num(node->data.output.output_type);
    print_str(" content_idx=");
    print_num(node->data.output.content_idx);
    print_str("\n");
    
    if (node->data.output.output_type == TOK_PRINT) {
        // Get content index
        uint32_t content_idx = node->data.output.content_idx;
        
        // Removed debug output
        
        if (content_idx != 0xFFFF) {
            // Check if this is a node index first
            bool is_node_index = false;
            if (content_idx < 4096 && content_idx > 0) {
                // Check if it's a valid node
                ASTNode* test_node = &nodes[content_idx];
                if (test_node->type > 0 && test_node->type < 100) { // Valid node types
                    is_node_index = true;
                }
            }
            
            bool is_string_literal = false;
            
            if (!is_node_index) {
                // Only check for string literal if it's not a valid node
                // First, let's see if treating it as a string pool offset gives valid data
                if (content_idx < 65536) { // Reasonable string pool size
                const char* test_str = &string_pool[content_idx];
                
                // Check if first few chars are printable
                bool looks_like_string = true;
                for (int i = 0; i < 5 && test_str[i]; i++) {
                    if (test_str[i] < 32 || test_str[i] > 126) {
                        looks_like_string = false;
                        break;
                    }
                }
                if (looks_like_string && test_str[0] != 0) {
                    is_string_literal = true;
                }
                }
            }
            
            if (is_string_literal && !is_node_index) {
                // This is a string pool offset for a literal string
                // Removed debug output
                
                // Get the string from the pool
                const char* str_content = &string_pool[content_idx];
                
                // Find string length (look for null terminator)
                uint32_t str_len = 0;
                while (str_content[str_len] != 0 && str_len < 1000) {
                    str_len++;
                }
                
                if (str_len > 0) {
                    generate_print(buf, str_content, str_len);
                    // Add newline
                    generate_print(buf, "\n", 1);
                } else {
                    const char* msg = "Empty string literal\n";
                    generate_print(buf, msg, 21);
                }
                return; // Done processing this print statement
            }
            
            // Otherwise it's a node index
            if (content_idx >= 4096) {
                print_str("[OUTPUT] Invalid node index ");
                print_num(content_idx);
                print_str("\n");
                return;
            }
            
            // content_idx is a node index - check what kind of node it is
            ASTNode* content_node = &nodes[content_idx];
            
            print_str("[OUTPUT] Processing content_idx=");
            print_num(content_idx);
            print_str(" with type=");
            print_num(content_node->type);
            print_str("\n");
            
            if (content_node->type == NODE_NUMBER) {
                // Print the number
                char num_buf[32];
                int64_t value = content_node->data.number;
                
                // Convert number to string
                int len = 0;
                if (value == 0) {
                    num_buf[len++] = '0';
                } else {
                    int64_t temp = value;
                    int start = len;
                    bool negative = false;
                    
                    if (temp < 0) {
                        negative = true;
                        temp = -temp;
                    }
                    
                    // Convert digits
                    while (temp > 0) {
                        num_buf[len++] = '0' + (temp % 10);
                        temp /= 10;
                    }
                    
                    if (negative) {
                        num_buf[len++] = '-';
                    }
                    
                    // Reverse the string
                    int end = len - 1;
                    while (start < end) {
                        char c = num_buf[start];
                        num_buf[start] = num_buf[end];
                        num_buf[end] = c;
                        start++;
                        end--;
                    }
                }
                
                // Print the number string
                generate_print(buf, num_buf, len);
                // Add newline
                generate_print(buf, "\n", 1);
            } else if (content_node->type == NODE_FLOAT) {
                // Generate the float value in XMM0
                generate_expression(buf, nodes, content_idx, symbols, string_pool);
                // Print the float from XMM0
                generate_print_float(buf);
            } else if (content_node->type == NODE_SOLID) {
                // Generate the solid number (pointer in RAX)
                generate_expression(buf, nodes, content_idx, symbols, string_pool);
                // Print the solid number from RAX
                extern void generate_print_solid(CodeBuffer* buf);
                generate_print_solid(buf);
                // No stack cleanup needed since we're not allocating yet
            } else if (content_node->type == NODE_BOOL) {
                // Boolean literal - print "true" or "false"
                bool value = content_node->data.boolean.value;
                if (value) {
                    generate_print(buf, "true", 4);
                } else {
                    generate_print(buf, "false", 5);
                }
            } else if (content_node->type == NODE_STRING) {
                // For string nodes, the name_offset points to string pool
                const char* str_content = &string_pool[content_node->data.ident.name_offset];
                uint32_t str_len = content_node->data.ident.name_len;
                
                if (str_len > 0) {
                    generate_print(buf, str_content, str_len);
                    // Add newline
                    generate_print(buf, "\n", 1);
                } else {
                    const char* msg = "Empty print content\n";
                    generate_print(buf, msg, 20);
                }
            } else if (content_node->type == NODE_BINARY_OP || 
                      content_node->type == NODE_IDENTIFIER ||
                      content_node->type == NODE_UNARY_OP ||
                      content_node->type == NODE_FUNC_CALL) {
                // For function calls, we need to generate the call first
                if (content_node->type == NODE_FUNC_CALL) {
                    generate_func_call(buf, nodes, content_idx, symbols, string_pool);
                    // Result is in RAX (integer) or XMM0 (float)
                    // For now, assume integer result - we'll improve this later
                    generate_print_number(buf, RAX);
                } else if (content_node->type == NODE_IDENTIFIER) {
                    // Special handling for identifiers - check variable type
                    char var_name[256];
                    uint32_t name_len = content_node->data.ident.name_len;
                    if (name_len >= 256) name_len = 255;
                    
                    for (uint32_t i = 0; i < name_len; i++) {
                        var_name[i] = string_pool[content_node->data.ident.name_offset + i];
                    }
                    var_name[name_len] = '\0';
                    
                    // Check variable type
                    VarEntry* var = get_or_create_var(var_name);
                    
                    print_str("[OUTPUT] Variable name: ");
                    print_str(var_name);
                    print_str(" var ptr: ");
                    print_num((unsigned long)var);
                    print_str(" type: ");
                    if (var) {
                        print_num(var->var_type);
                        print_str(" (VAR_TYPE_FLOAT=");
                        print_num(VAR_TYPE_FLOAT);
                        print_str(")\n");
                    } else {
                        print_str("NULL\n");
                    }
                    
                    if (var && var->var_type == VAR_TYPE_FLOAT) {
                        print_str("[OUTPUT] Variable is float type, calling generate_print_float\n");
                        // Generate identifier - will load float into XMM0
                        generate_expression(buf, nodes, content_idx, symbols, string_pool);
                        // Print the float from XMM0
                        generate_print_float(buf);
                    } else if (var && var->var_type == VAR_TYPE_SOLID) {
                        print_str("[OUTPUT] Variable is solid type, calling generate_print_solid\n");
                        // Generate identifier - will load solid pointer into RAX
                        generate_expression(buf, nodes, content_idx, symbols, string_pool);
                        // Print the solid number from RAX
                        extern void generate_print_solid(CodeBuffer* buf);
                        generate_print_solid(buf);
                        print_str("[OUTPUT] After generate_print_solid for variable\n");
                    } else if (var && var->var_type == VAR_TYPE_BOOL) {
                        print_str("[OUTPUT] Variable is bool type\n");
                        // Generate identifier - will load bool (0/1) into RAX
                        generate_expression(buf, nodes, content_idx, symbols, string_pool);
                        // Print "true" or "false" based on RAX value
                        // Test RAX
                        emit_test_reg_reg(buf, RAX, RAX);
                        // Jump if zero (false)
                        emit_byte(buf, 0x74); // JZ
                        emit_byte(buf, 0x0C); // Skip "true" and jump to "false"
                        
                        // Print "true"
                        generate_print(buf, "true", 4);
                        // Jump to end
                        emit_byte(buf, 0xEB); // JMP
                        emit_byte(buf, 0x0A); // Skip "false"
                        
                        // Print "false"
                        generate_print(buf, "false", 5);
                    } else {
                        // Generate identifier - will load int into RAX
                        generate_expression(buf, nodes, content_idx, symbols, string_pool);
                        // Print integer
                        generate_print_number(buf, RAX);
                    }
                } else {
                    // Check if this is a solid expression
                    if (is_solid_expression_impl(nodes, content_idx, string_pool)) {
                        // Generate the solid expression (pointer in RAX)
                        generate_expression(buf, nodes, content_idx, symbols, string_pool);
                        // Print the solid number from RAX
                        extern void generate_print_solid(CodeBuffer* buf);
                        generate_print_solid(buf);
                    } else if (is_float_expression(nodes, content_idx)) {
                        // Generate the float expression in XMM0
                        generate_expression(buf, nodes, content_idx, symbols, string_pool);
                        // Print the float from XMM0
                        generate_print_float(buf);
                    } else {
                        // Generate expression and convert result to string
                        generate_expression(buf, nodes, content_idx, symbols, string_pool);
                        
                        // RAX now contains the result - use the simpler print number function
                        generate_print_number(buf, RAX);
                    }
                }
            } else {
                // Unsupported content type - print debug info
                const char* msg = "Unsupported print type: ";
                generate_print(buf, msg, 24);
                // Load and print the node type as a number
                emit_mov_reg_imm64(buf, RAX, content_node->type);
                generate_print_number(buf, RAX);
                // Add newline
                generate_print(buf, "\n", 1);
            }
        } else {
            // No content parsed
            const char* msg = "Blaze print output\n";
            generate_print(buf, msg, 19);
        }
    }
}

// Generate code for a statement
void generate_statement(CodeBuffer* buf, ASTNode* nodes, uint16_t stmt_idx,
                        SymbolTable* symbols, char* string_pool) {
    // Validate buffer pointer
    if (!buf || !buf->code) {
        return;
    }
    
    if (stmt_idx == 0 || stmt_idx >= 4096) {
        print_str("generate_statement: invalid stmt_idx ");
        print_num(stmt_idx);
        print_str("\n");
        return;
    }
    
    // Add infinite loop detection
    static uint16_t visited_nodes[100];
    static uint16_t visited_count = 0;
    static uint16_t recursion_depth = 0;
    
    // Check for infinite loop
    for (uint16_t i = 0; i < visited_count; i++) {
        if (visited_nodes[i] == stmt_idx) {
            print_str("[STMT] ERROR: Infinite loop detected! Node ");
            print_num(stmt_idx);
            print_str(" already visited at depth ");
            print_num(recursion_depth);
            print_str("\n");
            return;
        }
    }
    
    // Add current node to visited list
    if (visited_count < 100) {
        visited_nodes[visited_count++] = stmt_idx;
    }
    
    recursion_depth++;
    
    // Debug for function generation removed - using tagged output instead
    print_str("[STMT] generate_statement called with idx=");
    print_num(stmt_idx);
    print_str(" type=");
    print_num(nodes[stmt_idx].type);
    print_str(" depth=");
    print_num(recursion_depth);
    print_str("\n");
    
    ASTNode* node = &nodes[stmt_idx];
    
    // Defensive check for node validity
    if (node->type > 100) {  // Assuming max valid node type is less than 100
        print_str("generate_statement: ERROR - invalid node type ");
        print_num(node->type);
        print_str(" at index ");
        print_num(stmt_idx);
        print_str("\n");
        return;
    }
    
    print_str("[STMT] About to enter switch for node ");
    print_num(stmt_idx);
    print_str("\n");
    
    switch (node->type) {
        case NODE_PROGRAM:
            // Generate code for all statements in the program
            {
                uint16_t stmt = node->data.binary.left_idx;
                while (stmt != 0 && stmt < 4096) {
                    generate_statement(buf, nodes, stmt, symbols, string_pool);
                    stmt = nodes[stmt].data.binary.right_idx;
                }
            }
            break;
            
        case NODE_VAR_DEF:
            print_str("[BASIC] Found NODE_VAR_DEF at index ");
            print_num(stmt_idx);
            print_str(" calling generate_var_def\n");
            print_str("[BASIC] About to call with nodes=");
            print_num((unsigned long)nodes);
            print_str(" idx=");
            print_num(stmt_idx);
            print_str("\n");
            generate_var_def(buf, nodes, stmt_idx, symbols, string_pool);
            print_str("[BASIC] Returned from generate_var_def\n");
            break;
            
        case NODE_OUTPUT:
            generate_output(buf, nodes, stmt_idx, symbols, string_pool);
            break;
            
        case NODE_ACTION_BLOCK:
            // Generate code for each action in the block
            {
                uint16_t action = node->data.binary.left_idx;
                while (action != 0 && action < 4096) {
                    generate_statement(buf, nodes, action, symbols, string_pool);
                    action = nodes[action].data.binary.right_idx;
                }
            }
            break;
            
        case NODE_FUNC_DEF:
            // Emit a jump over the function definition
            emit_byte(buf, 0xE9); // JMP rel32
            uint32_t jump_pos = buf->position;
            emit_dword(buf, 0); // Placeholder
            
            uint32_t func_start = buf->position;
            generate_func_def(buf, nodes, stmt_idx, symbols, string_pool);
            uint32_t func_end = buf->position;
            
            // Patch the jump to skip over the function
            int32_t jump_offset = func_end - (jump_pos + 4);
            *(int32_t*)&buf->code[jump_pos] = jump_offset;
            break;
            
        case NODE_FUNC_CALL:
            generate_func_call(buf, nodes, stmt_idx, symbols, string_pool);
            break;
            
        case NODE_CONDITIONAL:
            generate_conditional(buf, nodes, stmt_idx, symbols, string_pool);
            break;
            
        case NODE_DECLARE_BLOCK:
            // Declare blocks contain function definitions that should be handled
            // during the declare pass in main, not during regular statement generation
            // So we skip them here to avoid duplicate generation
            break;
            
        case NODE_RETURN: {
            print_str("[RETURN] WARNING: Found unexpected NODE_RETURN at idx=");
            print_num(stmt_idx);
            print_str(" - skipping since parser doesn't support return statements\n");
            
            // Since the parser doesn't intentionally create NODE_RETURN nodes,
            // this is likely a memory corruption or uninitialized node issue.
            // Skip processing this node to avoid segfault.
            // Add additional safety check to prevent any data access
            print_str("[RETURN] Node data structure appears corrupted - skipping safely\n");
            break;
        }
            
        default:
            // Skip other node types for now
            break;
    }
    
    print_str("[STMT] Completed processing node ");
    print_num(stmt_idx);
    print_str(" at depth ");
    print_num(recursion_depth);
    print_str("\n");
    
    // Clean up recursion tracking
    recursion_depth--;
    
    // Remove from visited list (for this recursion level)
    if (visited_count > 0) {
        visited_count--;
    }
}

// Generate code for conditional statements (if/else, while, etc.)
void generate_conditional(CodeBuffer* buf, ASTNode* nodes, uint16_t cond_idx,
                         SymbolTable* symbols, char* string_pool) {
    if (cond_idx == 0 || cond_idx >= 4096) {
        print_str("generate_conditional: invalid cond_idx ");
        print_num(cond_idx);
        print_str("\n");
        return;
    }
    
    ASTNode* cond_node = &nodes[cond_idx];
    if (cond_node->type != NODE_CONDITIONAL) {
        print_str("generate_conditional: not a conditional node\n");
        return;
    }
    
    print_str("[COND] Generating conditional with op=");
    print_num(cond_node->data.binary.op);
    print_str("\n");
    
    // Get the condition parameter and body
    uint16_t param_idx = cond_node->data.binary.left_idx;
    uint16_t body_idx = cond_node->data.binary.right_idx;
    
    // Generate the condition expression
    if (param_idx != 0 && param_idx < 4096) {
        generate_expression(buf, nodes, param_idx, symbols, string_pool);
        // Result is now in RAX
    }
    
    // For now, implement basic if/else logic
    // Test RAX (condition result)
    emit_test_reg_reg(buf, RAX, RAX);
    
    // Jump if zero (condition is false)
    emit_byte(buf, 0x74); // JZ
    uint32_t jump_pos = buf->position;
    emit_byte(buf, 0x00); // Placeholder offset
    
    // Generate the body (true case)
    if (body_idx != 0 && body_idx < 4096) {
        generate_statement(buf, nodes, body_idx, symbols, string_pool);
    }
    
    // Calculate jump offset to skip the body
    uint32_t body_end = buf->position;
    int8_t jump_offset = body_end - (jump_pos + 1);
    buf->code[jump_pos] = jump_offset;
    
    print_str("[COND] Conditional generation complete\n");
}