// Float to string conversion for x64 code generation

#include "blaze_internals.h"

// Forward declarations
extern void emit_mov_reg_imm64(CodeBuffer* buf, X64Register reg, uint64_t value);
extern void emit_mov_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_mov_mem_reg(CodeBuffer* buf, X64Register base, int32_t offset, X64Register src);
extern void emit_mov_reg_mem(CodeBuffer* buf, X64Register dst, X64Register base, int32_t offset);
extern void emit_add_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_sub_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_push_reg(CodeBuffer* buf, X64Register reg);
extern void emit_pop_reg(CodeBuffer* buf, X64Register reg);
extern void emit_syscall(CodeBuffer* buf);
extern void emit_byte(CodeBuffer* buf, uint8_t byte);
extern void emit_xor_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_cmp_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_jz(CodeBuffer* buf, int8_t offset);
extern void emit_jnz(CodeBuffer* buf, int8_t offset);
extern void emit_jmp_rel32(CodeBuffer* buf, int32_t offset);
extern void emit_jge_rel32(CodeBuffer* buf, int32_t offset);
extern void emit_inc_reg(CodeBuffer* buf, X64Register reg);
extern void emit_test_reg_reg(CodeBuffer* buf, X64Register reg1, X64Register reg2);
extern void emit_div_reg(CodeBuffer* buf, X64Register divisor);
extern void emit_mul_reg(CodeBuffer* buf, X64Register src);

// SSE register definitions
typedef enum {
    XMM0 = 0, XMM1 = 1, XMM2 = 2, XMM3 = 3,
    XMM4 = 4, XMM5 = 5, XMM6 = 6, XMM7 = 7
} SSERegister;

// Forward declarations for SSE instructions
extern void emit_movsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src);
extern void emit_movsd_xmm_mem(CodeBuffer* buf, SSERegister dst, X64Register base);
extern void emit_movsd_mem_xmm(CodeBuffer* buf, X64Register base, SSERegister src);
extern void emit_mulsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src);
extern void emit_divsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src);
extern void emit_cvtsd2si_reg_xmm(CodeBuffer* buf, X64Register dst, SSERegister src);
extern void emit_cvtsi2sd_xmm_reg(CodeBuffer* buf, SSERegister dst, X64Register src);
extern void emit_subsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src);

// Compare SSE registers
extern void emit_comisd_xmm_xmm(CodeBuffer* buf, SSERegister xmm1, SSERegister xmm2);

// Generate code to print a float from XMM0
void generate_print_float(CodeBuffer* buf) {
    // Save registers we'll use
    emit_push_reg(buf, RAX);
    emit_push_reg(buf, RBX);
    emit_push_reg(buf, RCX);
    emit_push_reg(buf, RDX);
    emit_push_reg(buf, RSI);
    emit_push_reg(buf, RDI);
    emit_push_reg(buf, R8);
    emit_push_reg(buf, R9);
    
    // Save XMM registers on stack
    emit_sub_reg_imm32(buf, RSP, 32); // Space for 4 doubles
    emit_movsd_mem_xmm(buf, RSP, XMM0);
    
    // Check for zero
    emit_xor_reg_reg(buf, RAX, RAX);
    emit_cvtsi2sd_xmm_reg(buf, XMM1, RAX); // XMM1 = 0.0
    emit_comisd_xmm_xmm(buf, XMM0, XMM1);
    uint32_t not_zero_jump = buf->position;
    emit_jnz(buf, 0); // placeholder
    
    // Handle zero case - print "0.0"
    emit_mov_reg_imm64(buf, RAX, '0');
    emit_push_reg(buf, RAX);
    emit_mov_reg_imm64(buf, RAX, 1);  // sys_write
    emit_mov_reg_imm64(buf, RDI, 1);  // stdout
    emit_mov_reg_reg(buf, RSI, RSP);
    emit_mov_reg_imm64(buf, RDX, 1);
    emit_syscall(buf);
    emit_add_reg_imm32(buf, RSP, 8);
    
    emit_mov_reg_imm64(buf, RAX, '.');
    emit_push_reg(buf, RAX);
    emit_mov_reg_imm64(buf, RAX, 1);  // sys_write
    emit_mov_reg_imm64(buf, RDI, 1);  // stdout
    emit_mov_reg_reg(buf, RSI, RSP);
    emit_mov_reg_imm64(buf, RDX, 1);
    emit_syscall(buf);
    emit_add_reg_imm32(buf, RSP, 8);
    
    emit_mov_reg_imm64(buf, RAX, '0');
    emit_push_reg(buf, RAX);
    emit_mov_reg_imm64(buf, RAX, 1);  // sys_write
    emit_mov_reg_imm64(buf, RDI, 1);  // stdout
    emit_mov_reg_reg(buf, RSI, RSP);
    emit_mov_reg_imm64(buf, RDX, 1);
    emit_syscall(buf);
    emit_add_reg_imm32(buf, RSP, 8);
    
    // Jump to end
    uint32_t to_end_from_zero = buf->position;
    emit_jmp_rel32(buf, 0); // placeholder
    
    // Patch not_zero jump
    uint8_t* patch_not_zero = &buf->code[not_zero_jump + 1];
    *patch_not_zero = buf->position - not_zero_jump - 2;
    
    // Check for negative
    emit_xor_reg_reg(buf, RAX, RAX);
    emit_cvtsi2sd_xmm_reg(buf, XMM1, RAX); // XMM1 = 0.0
    emit_comisd_xmm_xmm(buf, XMM0, XMM1);
    uint32_t positive_jump = buf->position;
    emit_byte(buf, 0x73); // jae (jump if above or equal)
    emit_byte(buf, 0); // placeholder
    
    // Handle negative - print minus sign
    emit_mov_reg_imm64(buf, RAX, '-');
    emit_push_reg(buf, RAX);
    emit_mov_reg_imm64(buf, RAX, 1);  // sys_write
    emit_mov_reg_imm64(buf, RDI, 1);  // stdout
    emit_mov_reg_reg(buf, RSI, RSP);
    emit_mov_reg_imm64(buf, RDX, 1);
    emit_syscall(buf);
    emit_add_reg_imm32(buf, RSP, 8);
    
    // Negate the value (XMM0 = -XMM0)
    // Load -0.0 into XMM1
    emit_mov_reg_imm64(buf, RAX, 0x8000000000000000ULL); // Sign bit
    emit_push_reg(buf, RAX);
    emit_movsd_xmm_mem(buf, XMM1, RSP);
    emit_add_reg_imm32(buf, RSP, 8);
    
    // XOR to flip sign bit
    emit_byte(buf, 0x66); // Prefix
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x57);
    emit_byte(buf, 0xC1); // xorpd xmm0, xmm1
    
    // Patch positive jump
    uint8_t* patch_positive = &buf->code[positive_jump + 1];
    *patch_positive = buf->position - positive_jump - 2;
    
    // For simplicity, print a fixed approximation
    // In a real implementation, we'd extract mantissa and exponent
    // For now, just print integer part, decimal point, and first few decimal places
    
    // Extract integer part
    emit_cvtsd2si_reg_xmm(buf, RAX, XMM0); // RAX = (int)XMM0
    emit_push_reg(buf, RAX); // Save integer part
    
    // Convert integer part to float and subtract from original
    emit_cvtsi2sd_xmm_reg(buf, XMM1, RAX);
    emit_movsd_xmm_xmm(buf, XMM2, XMM0);
    emit_subsd_xmm_xmm(buf, XMM2, XMM1); // XMM2 = fractional part
    
    // Print integer part
    emit_pop_reg(buf, RBX); // RBX = integer part
    
    // Check if integer part is 0
    emit_test_reg_reg(buf, RBX, RBX);
    uint32_t int_not_zero = buf->position;
    emit_jnz(buf, 0); // placeholder
    
    // Print single 0
    emit_mov_reg_imm64(buf, RAX, '0');
    emit_push_reg(buf, RAX);
    emit_mov_reg_imm64(buf, RAX, 1);  // sys_write
    emit_mov_reg_imm64(buf, RDI, 1);  // stdout
    emit_mov_reg_reg(buf, RSI, RSP);
    emit_mov_reg_imm64(buf, RDX, 1);
    emit_syscall(buf);
    emit_add_reg_imm32(buf, RSP, 8);
    
    uint32_t skip_int_print = buf->position;
    emit_jmp_rel32(buf, 0); // placeholder
    
    // Patch int_not_zero jump
    uint8_t* patch_int_not_zero = &buf->code[int_not_zero + 1];
    *patch_int_not_zero = buf->position - int_not_zero - 2;
    
    // Print integer part digits
    emit_xor_reg_reg(buf, RCX, RCX); // digit count
    emit_mov_reg_imm64(buf, R8, 10); // divisor
    
    // Extract digits
    uint32_t digit_loop = buf->position;
    emit_mov_reg_reg(buf, RAX, RBX);
    emit_xor_reg_reg(buf, RDX, RDX);
    emit_div_reg(buf, R8);
    emit_mov_reg_reg(buf, RBX, RAX); // quotient
    emit_add_reg_imm32(buf, RDX, '0'); // remainder to ASCII
    emit_push_reg(buf, RDX);
    emit_inc_reg(buf, RCX);
    emit_test_reg_reg(buf, RBX, RBX);
    int8_t loop_offset = digit_loop - (buf->position + 2);
    emit_jnz(buf, loop_offset);
    
    // Print digits in reverse order
    emit_mov_reg_reg(buf, RBX, RCX); // RBX = count
    
    uint32_t print_loop = buf->position;
    emit_test_reg_reg(buf, RBX, RBX);
    uint32_t print_done = buf->position;
    emit_jz(buf, 0); // placeholder
    
    emit_mov_reg_imm64(buf, RAX, 1);  // sys_write
    emit_mov_reg_imm64(buf, RDI, 1);  // stdout
    emit_mov_reg_reg(buf, RSI, RSP);
    emit_mov_reg_imm64(buf, RDX, 1);
    emit_syscall(buf);
    
    emit_add_reg_imm32(buf, RSP, 8);
    emit_sub_reg_imm32(buf, RBX, 1);
    
    int8_t print_loop_offset = print_loop - (buf->position + 2);
    emit_byte(buf, 0xEB);
    emit_byte(buf, print_loop_offset);
    
    // Patch print_done jump
    uint8_t* patch_print_done = &buf->code[print_done + 1];
    *patch_print_done = buf->position - print_done - 2;
    
    // Patch skip_int_print jump
    uint32_t* patch_skip_int = (uint32_t*)&buf->code[skip_int_print + 1];
    *patch_skip_int = buf->position - skip_int_print - 5;
    
    // Print decimal point
    emit_mov_reg_imm64(buf, RAX, '.');
    emit_push_reg(buf, RAX);
    emit_mov_reg_imm64(buf, RAX, 1);  // sys_write
    emit_mov_reg_imm64(buf, RDI, 1);  // stdout
    emit_mov_reg_reg(buf, RSI, RSP);
    emit_mov_reg_imm64(buf, RDX, 1);
    emit_syscall(buf);
    emit_add_reg_imm32(buf, RSP, 8);
    
    // Print 3 decimal places
    // Load 10.0 for multiplication
    emit_mov_reg_imm64(buf, RAX, 0x4024000000000000ULL); // 10.0
    emit_push_reg(buf, RAX);
    emit_movsd_xmm_mem(buf, XMM3, RSP);
    emit_add_reg_imm32(buf, RSP, 8);
    
    // XMM2 has fractional part
    emit_mov_reg_imm64(buf, RCX, 3); // 3 decimal places
    
    uint32_t decimal_loop = buf->position;
    // Multiply fractional part by 10
    emit_mulsd_xmm_xmm(buf, XMM2, XMM3);
    
    // Extract digit
    emit_cvtsd2si_reg_xmm(buf, RAX, XMM2);
    emit_add_reg_imm32(buf, RAX, '0');
    emit_push_reg(buf, RAX);
    emit_mov_reg_imm64(buf, RAX, 1);  // sys_write
    emit_mov_reg_imm64(buf, RDI, 1);  // stdout
    emit_mov_reg_reg(buf, RSI, RSP);
    emit_mov_reg_imm64(buf, RDX, 1);
    emit_syscall(buf);
    emit_add_reg_imm32(buf, RSP, 8);
    
    // Subtract integer part
    emit_cvtsd2si_reg_xmm(buf, RAX, XMM2);
    emit_cvtsi2sd_xmm_reg(buf, XMM1, RAX);
    emit_subsd_xmm_xmm(buf, XMM2, XMM1);
    
    emit_sub_reg_imm32(buf, RCX, 1);
    emit_test_reg_reg(buf, RCX, RCX);
    int8_t decimal_loop_offset = decimal_loop - (buf->position + 2);
    emit_jnz(buf, decimal_loop_offset);
    
    // Patch jump from zero case
    uint32_t* patch_zero_end = (uint32_t*)&buf->code[to_end_from_zero + 1];
    *patch_zero_end = buf->position - to_end_from_zero - 5;
    
    // Print newline
    emit_mov_reg_imm64(buf, RAX, '\n');
    emit_push_reg(buf, RAX);
    emit_mov_reg_imm64(buf, RAX, 1);  // sys_write
    emit_mov_reg_imm64(buf, RDI, 1);  // stdout
    emit_mov_reg_reg(buf, RSI, RSP);
    emit_mov_reg_imm64(buf, RDX, 1);
    emit_syscall(buf);
    emit_add_reg_imm32(buf, RSP, 8);
    
    // Restore XMM0
    emit_movsd_xmm_mem(buf, XMM0, RSP);
    emit_add_reg_imm32(buf, RSP, 32);
    
    // Restore registers
    emit_pop_reg(buf, R9);
    emit_pop_reg(buf, R8);
    emit_pop_reg(buf, RDI);
    emit_pop_reg(buf, RSI);
    emit_pop_reg(buf, RDX);
    emit_pop_reg(buf, RCX);
    emit_pop_reg(buf, RBX);
    emit_pop_reg(buf, RAX);
}