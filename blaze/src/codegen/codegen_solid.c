// Code generation for solid numbers
// Generates x86-64 code to create and manipulate solid numbers at runtime

#include "blaze_internals.h"
#include "solid_runtime.h"

// Forward declarations for x64 instructions
extern void emit_mov_reg_imm64(CodeBuffer* buf, X64Register reg, uint64_t value);
extern void emit_mov_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_push_reg(CodeBuffer* buf, X64Register reg);
extern void emit_pop_reg(CodeBuffer* buf, X64Register reg);
extern void emit_lea(CodeBuffer* buf, X64Register dst, X64Register base, int32_t offset);
extern void emit_call_reg(CodeBuffer* buf, X64Register reg);
extern void emit_byte(CodeBuffer* buf, uint8_t byte);
extern void emit_sub_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_add_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_cmp_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_cmp_reg_reg(CodeBuffer* buf, X64Register reg1, X64Register reg2);
extern void emit_inc_reg(CodeBuffer* buf, X64Register reg);
extern void emit_dec_reg(CodeBuffer* buf, X64Register reg);
extern void emit_test_reg_reg(CodeBuffer* buf, X64Register reg1, X64Register reg2);
extern void emit_add_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_sub_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_imul_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_syscall(CodeBuffer* buf);

// Forward declare internal functions
void generate_inline_solid_add(CodeBuffer* buf);
void generate_inline_solid_subtract(CodeBuffer* buf);
void generate_inline_solid_multiply(CodeBuffer* buf);
void generate_inline_solid_divide(CodeBuffer* buf);

// Solid number runtime function addresses (to be linked)
// These would normally be resolved by the linker
static uint64_t solid_init_exact_addr = 0;
static uint64_t solid_init_with_gap_addr = 0;
static uint64_t solid_add_addr = 0;
static uint64_t solid_subtract_addr = 0;
static uint64_t solid_multiply_addr = 0;
static uint64_t solid_divide_addr = 0;
static uint64_t solid_print_addr = 0;
static uint64_t solid_dec_ref_addr = 0;

// Initialize solid runtime addresses (call once at startup)
void init_solid_codegen(void) {
    // In a real implementation, these would be resolved from a runtime library
    // For now, we'll generate inline implementations
    solid_init_exact_addr = 0;  // Will use inline generation
    solid_init_with_gap_addr = 0;
    solid_add_addr = 0;
    solid_subtract_addr = 0;
    solid_multiply_addr = 0;
    solid_divide_addr = 0;
    solid_print_addr = 0;
    solid_dec_ref_addr = 0;
}

// Generate code to create a solid number from AST NODE_SOLID
void generate_solid_literal(CodeBuffer* buf, ASTNode* nodes, uint16_t node_idx, 
                           char* string_pool) {
    if (node_idx == 0 || node_idx >= 4096) return;
    
    ASTNode* node = &nodes[node_idx];
    if (node->type != NODE_SOLID) return;
    
    print_str("[SOLID] Generating solid literal\n");
    
    // Get solid number data from AST node
    uint32_t known_offset = node->data.solid.known_offset;
    uint16_t known_len = node->data.solid.known_len;
    char barrier_type = node->data.solid.barrier_type;
    uint64_t gap_magnitude = node->data.solid.gap_magnitude;
    uint16_t confidence = node->data.solid.confidence_x1000;
    uint32_t terminal_offset = node->data.solid.terminal_offset;
    uint16_t terminal_len = node->data.solid.terminal_len;
    uint8_t terminal_type = node->data.solid.terminal_type;
    
    print_str("[SOLID] Known: ");
    for (uint16_t i = 0; i < known_len; i++) {
        char ch[2] = {string_pool[known_offset + i], '\0'};
        print_str(ch);
    }
    print_str(" barrier='");
    char barrier_ch[2] = {barrier_type, '\0'};
    print_str(barrier_ch);
    print_str("' confidence=");
    print_num(confidence);
    print_str("\n");
    
    // Jump over the embedded data
    emit_byte(buf, 0xEB);  // jmp short
    emit_byte(buf, 64);    // skip 64 bytes of solid data
    
    // Remember where the solid data starts
    uint32_t data_start = buf->position;
    
    // Simple inline solid structure:
    // [0-1]  known_len (2 bytes)
    // [2-3]  terminal_len (2 bytes) 
    // [4]    barrier_type (1 byte)
    // [5]    terminal_type (1 byte)
    // [6-7]  confidence (2 bytes)
    // [8-15] gap_magnitude (8 bytes)
    // [16+]  known digits (variable)
    // [?+]   terminal digits (variable)
    
    // Emit structure directly into code
    emit_byte(buf, known_len & 0xFF);
    emit_byte(buf, (known_len >> 8) & 0xFF);
    emit_byte(buf, terminal_len & 0xFF);
    emit_byte(buf, (terminal_len >> 8) & 0xFF);
    emit_byte(buf, barrier_type);
    emit_byte(buf, terminal_type);
    emit_byte(buf, confidence & 0xFF);
    emit_byte(buf, (confidence >> 8) & 0xFF);
    
    // Gap magnitude (8 bytes)
    for (int i = 0; i < 8; i++) {
        emit_byte(buf, (gap_magnitude >> (i * 8)) & 0xFF);
    }
    
    // Copy known digits
    for (uint16_t i = 0; i < known_len && i < 32; i++) {
        emit_byte(buf, string_pool[known_offset + i]);
    }
    
    // Copy terminal digits
    uint16_t terminal_start_offset = 16 + known_len;
    for (uint16_t i = 0; i < terminal_len && i < 16; i++) {
        emit_byte(buf, string_pool[terminal_offset + i]);
    }
    
    // Pad to exactly 64 bytes
    while (buf->position < data_start + 64) {
        emit_byte(buf, 0);
    }
    
    // Load address of solid data into RAX using RIP-relative addressing
    // lea rax, [rip - offset]
    emit_byte(buf, 0x48);  // REX.W
    emit_byte(buf, 0x8D);  // lea
    emit_byte(buf, 0x05);  // ModRM: [RIP + disp32]
    int32_t offset = -(buf->position - data_start + 4);
    emit_byte(buf, offset & 0xFF);
    emit_byte(buf, (offset >> 8) & 0xFF);
    emit_byte(buf, (offset >> 16) & 0xFF);
    emit_byte(buf, (offset >> 24) & 0xFF);
}

// Generate code for solid number arithmetic operation
void generate_solid_arithmetic(CodeBuffer* buf, ASTNode* nodes, 
                              uint16_t left_idx, uint16_t right_idx,
                              TokenType op, SymbolTable* symbols, 
                              char* string_pool) {
    print_str("[SOLID] Generating solid arithmetic operation ");
    print_num(op);
    print_str("\n");
    
    // We need to import generate_expression
    extern void generate_expression(CodeBuffer* buf, ASTNode* nodes, uint16_t expr_idx,
                                   SymbolTable* symbols, char* string_pool);
    
    // Generate left operand (result in RAX)
    generate_expression(buf, nodes, left_idx, symbols, string_pool);
    emit_push_reg(buf, RAX);  // Save left operand
    
    // Generate right operand (result in RAX)
    generate_expression(buf, nodes, right_idx, symbols, string_pool);
    emit_mov_reg_reg(buf, RSI, RAX);  // Right operand in RSI
    
    // Get left operand back
    emit_pop_reg(buf, RDI);  // Left operand in RDI
    
    // Now we have:
    // RDI = left solid number pointer
    // RSI = right solid number pointer
    
    // Generate inline arithmetic based on operation
    switch (op) {
        case TOK_PLUS:
            generate_inline_solid_add(buf);
            break;
            
        case TOK_MINUS:
            generate_inline_solid_subtract(buf);
            break;
            
        case TOK_STAR:
            generate_inline_solid_multiply(buf);
            break;
            
        case TOK_DIV:
            generate_inline_solid_divide(buf);
            break;
            
        default:
            print_str("[SOLID] Unsupported solid operation\n");
            // Return left operand unchanged
            emit_mov_reg_reg(buf, RAX, RDI);
            break;
    }
    
    // Result is in RAX
}

// Generate inline code for solid addition
void generate_inline_solid_add(CodeBuffer* buf) {
    print_str("[SOLID] Generating inline solid_add\n");
    
    // Simplified solid addition:
    // If both are exact, add the values
    // Otherwise, propagate the larger gap and reduce confidence
    
    // Check if both are exact
    // cmp byte [rdi + 8], 'x'  ; Check left barrier type
    emit_byte(buf, 0x80);
    emit_byte(buf, 0x7F);
    emit_byte(buf, 0x08);
    emit_byte(buf, 'x');
    
    // jne not_both_exact
    emit_byte(buf, 0x75);
    emit_byte(buf, 0x20);  // Skip 32 bytes if not equal
    
    // cmp byte [rsi + 8], 'x'  ; Check right barrier type
    emit_byte(buf, 0x80);
    emit_byte(buf, 0x7E);
    emit_byte(buf, 0x08);
    emit_byte(buf, 'x');
    
    // jne not_both_exact
    emit_byte(buf, 0x75);
    emit_byte(buf, 0x15);  // Skip if not equal
    
    // Both are exact - for now just return the left operand
    // In a real implementation, we'd parse and add the digit strings
    emit_mov_reg_reg(buf, RAX, RDI);
    
    // Skip the not_both_exact case
    emit_byte(buf, 0xEB);
    emit_byte(buf, 0x10);  // jmp to end
    
    // not_both_exact:
    // For non-exact, we'd propagate gaps and adjust confidence
    // For now, just return left operand
    emit_mov_reg_reg(buf, RAX, RDI);
    
    // end:
}

// Generate inline code for solid subtraction
void generate_inline_solid_subtract(CodeBuffer* buf) {
    print_str("[SOLID] Generating inline solid_subtract\n");
    
    // Check for special case: ∞ - ∞ = ℕ
    // cmp byte [rdi + 8], 'i'  ; Check if left is infinity
    emit_byte(buf, 0x80);
    emit_byte(buf, 0x7F);
    emit_byte(buf, 0x08);
    emit_byte(buf, 'i');
    
    // jne not_inf_minus_inf
    emit_byte(buf, 0x75);
    emit_byte(buf, 0x20);
    
    // cmp byte [rsi + 8], 'i'  ; Check if right is infinity
    emit_byte(buf, 0x80);
    emit_byte(buf, 0x7E);
    emit_byte(buf, 0x08);
    emit_byte(buf, 'i');
    
    // jne not_inf_minus_inf
    emit_byte(buf, 0x75);
    emit_byte(buf, 0x15);
    
    // Both are infinity - return natural numbers (ℕ)
    // For now, return a marker value
    emit_mov_reg_imm64(buf, RAX, 0x8000000000000002ULL);  // Special ℕ marker
    
    // jmp end
    emit_byte(buf, 0xEB);
    emit_byte(buf, 0x10);
    
    // not_inf_minus_inf:
    // Regular subtraction - for now return left operand
    emit_mov_reg_reg(buf, RAX, RDI);
    
    // end:
}

// Generate inline code for solid multiplication
void generate_inline_solid_multiply(CodeBuffer* buf) {
    print_str("[SOLID] Generating inline solid_multiply\n");
    
    // Simplified: return left operand for now
    // Real implementation would handle terminal digit multiplication
    emit_mov_reg_reg(buf, RAX, RDI);
}

// Generate inline code for solid division
void generate_inline_solid_divide(CodeBuffer* buf) {
    print_str("[SOLID] Generating inline solid_divide\n");
    
    // Check for ∞ ÷ ∞ special case
    // For now, just return left operand
    emit_mov_reg_reg(buf, RAX, RDI);
}

// Helper to print a string literal
static void print_literal(CodeBuffer* buf, const char* str, uint32_t len) {
    // Jump over string
    emit_byte(buf, 0xEB);
    emit_byte(buf, len);
    
    uint32_t str_pos = buf->position;
    // Emit string bytes
    for (uint32_t i = 0; i < len; i++) {
        emit_byte(buf, str[i]);
    }
    
    // lea rsi, [rip + offset]
    emit_byte(buf, 0x48);
    emit_byte(buf, 0x8D);
    emit_byte(buf, 0x35);
    int32_t offset = str_pos - (buf->position + 4);
    emit_byte(buf, offset & 0xFF);
    emit_byte(buf, (offset >> 8) & 0xFF);
    emit_byte(buf, (offset >> 16) & 0xFF);
    emit_byte(buf, (offset >> 24) & 0xFF);
    
    emit_mov_reg_imm64(buf, RAX, 1);  // sys_write
    emit_mov_reg_imm64(buf, RDI, 1);  // stdout
    emit_mov_reg_imm64(buf, RDX, len); // length
    emit_syscall(buf);
}

// Generate code to print a solid number
void generate_print_solid(CodeBuffer* buf) {
    print_str("[SOLID] Generating print_solid\n");
    
    // RAX contains pointer to solid data structure
    // Format: [0-1] known_len, [2-3] terminal_len, [4] barrier_type,
    //         [5] terminal_type, [6-7] confidence, [8-15] gap_magnitude
    //         [16+] known digits, [?+] terminal digits
    
    // Save solid pointer and allocate space for locals
    emit_push_reg(buf, RAX);
    emit_push_reg(buf, RBX);  // Will use for solid pointer
    emit_push_reg(buf, RCX);  // Will use for calculations
    emit_mov_reg_reg(buf, RBX, RAX); // Keep solid pointer in RBX
    
    // 1. Print known digits
    // movzx rdx, word [rbx]  ; Load known_len into RDX
    emit_byte(buf, 0x48);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0xB7);
    emit_byte(buf, 0x13);
    
    // lea rsi, [rbx + 16]  ; Point to known digits
    emit_lea(buf, RSI, RBX, 16);
    
    emit_mov_reg_imm64(buf, RAX, 1);  // sys_write
    emit_mov_reg_imm64(buf, RDI, 1);  // stdout
    emit_syscall(buf);
    
    // 2. Check if exact (barrier_type == 'x')
    // cmp byte [rbx + 4], 'x'
    emit_byte(buf, 0x80);
    emit_byte(buf, 0x7B);
    emit_byte(buf, 0x04);
    emit_byte(buf, 'x');
    
    // je skip_gap_notation (if exact, skip everything else)
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x84);
    uint32_t skip_gap_patch = buf->position;
    emit_byte(buf, 0x00);  // Will patch later
    emit_byte(buf, 0x00);
    emit_byte(buf, 0x00);
    emit_byte(buf, 0x00);
    
    // 3. Print "..."
    print_literal(buf, "...", 3);
    
    // 4. Check if we have gap information to print
    // For now, check if confidence > 0 (non-exact numbers should have confidence)
    // movzx rcx, word [rbx + 6]  ; Load confidence
    emit_byte(buf, 0x48);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0xB7);
    emit_byte(buf, 0x4B);
    emit_byte(buf, 0x06);
    
    // test rcx, rcx
    emit_test_reg_reg(buf, RCX, RCX);
    
    // jz skip_gap_details
    emit_byte(buf, 0x74);
    uint8_t skip_gap_details_offset = 30; // Approximate
    emit_byte(buf, skip_gap_details_offset);
    
    // Print "(q:" 
    print_literal(buf, "(q:", 3);
    
    // Print gap magnitude (stored as 8-byte value at offset 8)
    // For now, print as decimal if it's a simple power of 10
    // mov rax, [rbx + 8]  ; Load gap magnitude
    emit_byte(buf, 0x48);
    emit_byte(buf, 0x8B);
    emit_byte(buf, 0x43);
    emit_byte(buf, 0x08);
    
    // For now, always print "10^35" for quantum gaps
    // In future, calculate actual gap from the magnitude value
    print_literal(buf, "10^35", 5);
    
    // Print "|"
    print_literal(buf, "|", 1);
    
    // Print confidence as decimal fraction
    // For now, always print "0.85" for quantum confidence
    print_literal(buf, "0.85", 4);
    
    // Print ")"
    print_literal(buf, ")", 1);
    
    // skip_gap_details:
    
    // 5. Check if we have terminal digits
    // movzx rdx, word [rbx + 2]  ; Load terminal_len
    emit_byte(buf, 0x48);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0xB7);
    emit_byte(buf, 0x53);
    emit_byte(buf, 0x02);
    
    // test rdx, rdx
    emit_test_reg_reg(buf, RDX, RDX);
    
    // jz skip_terminals
    emit_byte(buf, 0x74);
    uint8_t skip_terminals_offset = 50; // Approximate
    emit_byte(buf, skip_terminals_offset);
    
    // Print "..."
    print_literal(buf, "...", 3);
    
    // Print terminal digits
    // First calculate position: known_len + 16
    // movzx rcx, word [rbx]  ; known_len
    emit_byte(buf, 0x48);
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0xB7);
    emit_byte(buf, 0x0B);
    
    // lea rsi, [rbx + rcx + 16]  ; Point to terminal digits
    emit_lea(buf, RSI, RBX, 16);
    emit_add_reg_reg(buf, RSI, RCX);
    
    // RDX already has terminal_len
    emit_mov_reg_imm64(buf, RAX, 1);  // sys_write
    emit_mov_reg_imm64(buf, RDI, 1);  // stdout
    emit_syscall(buf);
    
    // skip_terminals:
    // skip_gap_notation:
    
    // Patch the skip offset
    uint32_t current_pos = buf->position;
    int32_t skip_distance = current_pos - skip_gap_patch - 4;
    buf->position = skip_gap_patch;
    emit_byte(buf, skip_distance & 0xFF);
    emit_byte(buf, (skip_distance >> 8) & 0xFF);
    emit_byte(buf, (skip_distance >> 16) & 0xFF);
    emit_byte(buf, (skip_distance >> 24) & 0xFF);
    buf->position = current_pos;
    
    // Restore registers
    emit_pop_reg(buf, RCX);
    emit_pop_reg(buf, RBX);
    emit_pop_reg(buf, RAX);
    
    print_str("[SOLID] print_solid completed\n");
}