// Float to string conversion and printing
#include "blaze_internals.h"

// SSE register definitions


// Forward declarations
extern void emit_byte(CodeBuffer* buf, uint8_t byte);
extern void emit_mov_reg_imm64(CodeBuffer* buf, X64Register reg, uint64_t value);
extern void emit_push_reg(CodeBuffer* buf, X64Register reg);
extern void emit_pop_reg(CodeBuffer* buf, X64Register reg);
extern void emit_mov_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_add_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_syscall(CodeBuffer* buf);
extern void emit_platform_print_char(CodeBuffer* buf, Platform platform);

// Forward declarations for float conversion
extern void emit_sub_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_cvtsd2si_reg_xmm(CodeBuffer* buf, X64Register dst, SSERegister src);
extern void emit_cvtsi2sd_xmm_reg(CodeBuffer* buf, SSERegister dst, X64Register src);
extern void emit_subsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src);
extern void emit_mulsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src);
extern void emit_movsd_xmm_imm(CodeBuffer* buf, SSERegister reg, double value);
extern void emit_movsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src);
extern void emit_movsd_mem_xmm(CodeBuffer* buf, X64Register base, SSERegister src);
extern void emit_cmp_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_jge_rel32(CodeBuffer* buf, int32_t offset);
extern void emit_neg_reg(CodeBuffer* buf, X64Register reg);
extern void emit_test_reg_reg(CodeBuffer* buf, X64Register reg1, X64Register reg2);
extern void emit_jz(CodeBuffer* buf, int8_t offset);
extern void emit_div_reg(CodeBuffer* buf, X64Register divisor);
extern void emit_xor_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_jnz(CodeBuffer* buf, int8_t offset);
extern void emit_inc_reg(CodeBuffer* buf, X64Register reg);
extern void emit_mov_mem_reg(CodeBuffer* buf, X64Register base, int32_t offset, X64Register src);
extern void emit_mov_reg_mem(CodeBuffer* buf, X64Register dst, X64Register base, int32_t offset);

// Generate code to print a float from XMM0
void generate_print_float(CodeBuffer* buf) {
    // Save all registers we'll use
    emit_push_reg(buf, RAX);
    emit_push_reg(buf, RBX);
    emit_push_reg(buf, RCX);
    emit_push_reg(buf, RDX);
    emit_push_reg(buf, RDI);
    emit_push_reg(buf, RSI);
    emit_push_reg(buf, R8);
    emit_push_reg(buf, R9);
    
    // Save XMM registers we'll use
    emit_sub_reg_imm32(buf, RSP, 32);
    // movsd [rsp], xmm0
    emit_byte(buf, 0xF2); emit_byte(buf, 0x0F); emit_byte(buf, 0x11); emit_byte(buf, 0x04); emit_byte(buf, 0x24);
    // movsd [rsp+8], xmm1
    emit_byte(buf, 0xF2); emit_byte(buf, 0x0F); emit_byte(buf, 0x11); emit_byte(buf, 0x4C); emit_byte(buf, 0x24); emit_byte(buf, 0x08);
    // movsd [rsp+16], xmm2
    emit_byte(buf, 0xF2); emit_byte(buf, 0x0F); emit_byte(buf, 0x11); emit_byte(buf, 0x54); emit_byte(buf, 0x24); emit_byte(buf, 0x10);
    
    // Copy XMM0 to XMM1 for processing
    emit_movsd_xmm_xmm(buf, XMM1, XMM0);
    
    // Check if negative by examining the sign bit
    emit_xor_reg_reg(buf, R8, R8); // R8 = 0 (sign flag)
    
    // Store XMM1 to check sign bit
    emit_sub_reg_imm32(buf, RSP, 8);
    emit_movsd_mem_xmm(buf, RSP, XMM1);
    emit_mov_reg_mem(buf, RAX, RSP, 0);
    emit_add_reg_imm32(buf, RSP, 8);
    
    // Check sign bit (bit 63)
    emit_test_reg_reg(buf, RAX, RAX);
    uint32_t positive_jump = buf->position;
    emit_jge_rel32(buf, 0); // placeholder
    
    // Handle negative: print minus and negate
    emit_mov_reg_imm64(buf, R8, 1); // Set sign flag
    emit_mov_reg_imm64(buf, RAX, '-');
    emit_push_reg(buf, RAX);
    emit_platform_print_char(buf, buf->target_platform);
    emit_add_reg_imm32(buf, RSP, 8);
    
    // Negate XMM1 (multiply by -1.0)
    emit_movsd_xmm_imm(buf, XMM2, -1.0);
    emit_mulsd_xmm_xmm(buf, XMM1, XMM2);
    
    // Patch positive jump
    uint32_t* patch_positive = (uint32_t*)&buf->code[positive_jump + 2];
    *patch_positive = buf->position - positive_jump - 6;
    
    // Extract integer part with truncation (not rounding)
    // We need to use cvttsd2si (with two 't's) for truncation
    // cvttsd2si rbx, xmm1
    emit_byte(buf, 0xF2); // SD prefix
    emit_byte(buf, 0x48); // REX.W for 64-bit
    emit_byte(buf, 0x0F); 
    emit_byte(buf, 0x2C); // cvttsd2si opcode
    emit_byte(buf, 0xD9); // ModRM: RBX, XMM1
    
    // Save the integer part for later use with fractional calculation
    emit_push_reg(buf, RBX);
    
    // Print integer part using existing number printing logic
    emit_mov_reg_reg(buf, RAX, RBX);
    
    // Special case for 0
    emit_test_reg_reg(buf, RAX, RAX);
    uint32_t not_zero_jump = buf->position;
    emit_jnz(buf, 0); // placeholder
    
    // Print '0'
    emit_mov_reg_imm64(buf, RAX, '0');
    emit_push_reg(buf, RAX);
    emit_platform_print_char(buf, buf->target_platform);
    emit_add_reg_imm32(buf, RSP, 8);
    
    // Jump to decimal point
    uint32_t to_decimal_from_zero = buf->position;
    emit_byte(buf, 0xE9); // jmp near
    emit_byte(buf, 0x00); emit_byte(buf, 0x00); emit_byte(buf, 0x00); emit_byte(buf, 0x00);
    
    // Patch not_zero jump
    uint8_t* patch_not_zero = &buf->code[not_zero_jump + 1];
    *patch_not_zero = buf->position - not_zero_jump - 2;
    
    // Print integer part digits
    emit_xor_reg_reg(buf, RCX, RCX); // digit count
    emit_mov_reg_imm64(buf, R9, 10); // divisor
    
    // Extract digits loop
    uint32_t digit_loop_start = buf->position;
    emit_xor_reg_reg(buf, RDX, RDX);
    emit_div_reg(buf, R9);
    emit_add_reg_imm32(buf, RDX, '0');
    emit_sub_reg_imm32(buf, RSP, 8);
    emit_mov_mem_reg(buf, RSP, 0, RDX);
    emit_inc_reg(buf, RCX);
    emit_test_reg_reg(buf, RAX, RAX);
    int8_t loop_offset = digit_loop_start - (buf->position + 2);
    emit_jnz(buf, loop_offset);
    
    // Print digits
    emit_mov_reg_reg(buf, RBX, RCX); // save count
    uint32_t print_loop_start = buf->position;
    emit_test_reg_reg(buf, RBX, RBX);
    uint32_t print_done_jump = buf->position;
    emit_jz(buf, 0); // placeholder
    
    emit_platform_print_char(buf, buf->target_platform);
    emit_add_reg_imm32(buf, RSP, 8);
    emit_sub_reg_imm32(buf, RBX, 1);
    int8_t print_loop_offset = print_loop_start - (buf->position + 2);
    emit_byte(buf, 0xEB);
    emit_byte(buf, print_loop_offset);
    
    // Patch print done jump
    uint8_t* patch_print_done = &buf->code[print_done_jump + 1];
    *patch_print_done = buf->position - print_done_jump - 2;
    
    // Patch jump from zero case
    uint32_t* patch_zero_end = (uint32_t*)&buf->code[to_decimal_from_zero + 1];
    *patch_zero_end = buf->position - to_decimal_from_zero - 5;
    
    // Print decimal point
    emit_mov_reg_imm64(buf, RAX, '.');
    emit_push_reg(buf, RAX);
    emit_platform_print_char(buf, buf->target_platform);
    emit_add_reg_imm32(buf, RSP, 8);
    
    // Restore integer part from stack
    emit_pop_reg(buf, RBX);
    
    // Calculate fractional part
    // Convert integer part back to float
    emit_cvtsi2sd_xmm_reg(buf, XMM2, RBX);
    // Subtract from original (still in XMM1) to get fraction
    emit_movsd_xmm_xmm(buf, XMM0, XMM1); // Copy original
    emit_subsd_xmm_xmm(buf, XMM0, XMM2); // XMM0 = fractional part
    
    // Multiply by 10 to get first decimal digit
    emit_movsd_xmm_imm(buf, XMM2, 10.0);
    emit_mulsd_xmm_xmm(buf, XMM0, XMM2);
    
    // Convert to integer (truncate) to get first decimal digit
    // cvttsd2si rax, xmm0
    emit_byte(buf, 0xF2); 
    emit_byte(buf, 0x48); 
    emit_byte(buf, 0x0F); 
    emit_byte(buf, 0x2C); 
    emit_byte(buf, 0xC0); // RAX, XMM0
    
    // Save first digit
    emit_push_reg(buf, RAX);
    
    // Print first decimal digit
    emit_add_reg_imm32(buf, RAX, '0');
    emit_push_reg(buf, RAX);
    emit_platform_print_char(buf, buf->target_platform);
    emit_add_reg_imm32(buf, RSP, 8);
    
    // Restore first digit
    emit_pop_reg(buf, RAX);
    
    // Convert first digit back to float and subtract
    emit_cvtsi2sd_xmm_reg(buf, XMM2, RAX);
    emit_subsd_xmm_xmm(buf, XMM0, XMM2);
    
    // Multiply remainder by 10 for second digit
    emit_movsd_xmm_imm(buf, XMM2, 10.0);
    emit_mulsd_xmm_xmm(buf, XMM0, XMM2);
    
    // Convert to integer (truncate) to get second decimal digit
    // cvttsd2si rax, xmm0
    emit_byte(buf, 0xF2); 
    emit_byte(buf, 0x48); 
    emit_byte(buf, 0x0F); 
    emit_byte(buf, 0x2C); 
    emit_byte(buf, 0xC0); // RAX, XMM0
    
    // Print second decimal digit
    emit_add_reg_imm32(buf, RAX, '0');
    emit_push_reg(buf, RAX);
    emit_platform_print_char(buf, buf->target_platform);
    emit_add_reg_imm32(buf, RSP, 8);
    
    // Print newline
    emit_mov_reg_imm64(buf, RAX, '\n');
    emit_push_reg(buf, RAX);
    emit_platform_print_char(buf, buf->target_platform);
    emit_add_reg_imm32(buf, RSP, 8);
    
    // Restore XMM registers
    // movsd xmm2, [rsp+16]
    emit_byte(buf, 0xF2); emit_byte(buf, 0x0F); emit_byte(buf, 0x10); emit_byte(buf, 0x54); emit_byte(buf, 0x24); emit_byte(buf, 0x10);
    // movsd xmm1, [rsp+8]
    emit_byte(buf, 0xF2); emit_byte(buf, 0x0F); emit_byte(buf, 0x10); emit_byte(buf, 0x4C); emit_byte(buf, 0x24); emit_byte(buf, 0x08);
    // movsd xmm0, [rsp]
    emit_byte(buf, 0xF2); emit_byte(buf, 0x0F); emit_byte(buf, 0x10); emit_byte(buf, 0x04); emit_byte(buf, 0x24);
    emit_add_reg_imm32(buf, RSP, 32);
    
    // Restore registers
    emit_pop_reg(buf, R9);
    emit_pop_reg(buf, R8);
    emit_pop_reg(buf, RSI);
    emit_pop_reg(buf, RDI);
    emit_pop_reg(buf, RDX);
    emit_pop_reg(buf, RCX);
    emit_pop_reg(buf, RBX);
    emit_pop_reg(buf, RAX);
}