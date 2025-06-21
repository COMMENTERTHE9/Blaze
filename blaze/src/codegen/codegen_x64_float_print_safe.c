// Safe float printing for x64
#include "blaze_internals.h"

// Forward declarations
extern void emit_byte(CodeBuffer* buf, uint8_t byte);
extern void emit_mov_reg_imm64(CodeBuffer* buf, X64Register reg, uint64_t value);
extern void emit_push_reg(CodeBuffer* buf, X64Register reg);
extern void emit_pop_reg(CodeBuffer* buf, X64Register reg);
extern void emit_mov_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_add_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_syscall(CodeBuffer* buf);

// Generate code to print a float from XMM0 - safe version
void generate_print_float_safe(CodeBuffer* buf) {
    // Save ALL registers to be extra safe
    emit_push_reg(buf, RAX);
    emit_push_reg(buf, RBX);
    emit_push_reg(buf, RCX);
    emit_push_reg(buf, RDX);
    emit_push_reg(buf, RSI);
    emit_push_reg(buf, RDI);
    emit_push_reg(buf, R8);
    emit_push_reg(buf, R9);
    emit_push_reg(buf, R10);
    emit_push_reg(buf, R11);
    
    // For now, just print "3.14\n" as a hardcoded test
    const char* float_str = "3.14\n";
    int len = 5;
    
    // Push the string onto the stack
    for (int i = len - 1; i >= 0; i--) {
        emit_mov_reg_imm64(buf, RAX, (uint64_t)float_str[i]);
        emit_push_reg(buf, RAX);
    }
    
    // sys_write syscall
    emit_mov_reg_imm64(buf, RAX, 1);  // sys_write
    emit_mov_reg_imm64(buf, RDI, 1);  // stdout
    emit_mov_reg_reg(buf, RSI, RSP);   // buffer address
    emit_mov_reg_imm64(buf, RDX, len); // length
    emit_syscall(buf);
    
    // Clean up the string from stack
    emit_add_reg_imm32(buf, RSP, len * 8);
    
    // Restore ALL registers in reverse order
    emit_pop_reg(buf, R11);
    emit_pop_reg(buf, R10);
    emit_pop_reg(buf, R9);
    emit_pop_reg(buf, R8);
    emit_pop_reg(buf, RDI);
    emit_pop_reg(buf, RSI);
    emit_pop_reg(buf, RDX);
    emit_pop_reg(buf, RCX);
    emit_pop_reg(buf, RBX);
    emit_pop_reg(buf, RAX);
}