// Exit code generation
#include "blaze_internals.h"

// Forward declarations
extern void emit_byte(CodeBuffer* buf, uint8_t byte);
extern void emit_mov_reg_imm64(CodeBuffer* buf, X64Register reg, uint64_t value);
extern void emit_syscall(CodeBuffer* buf);

// Generate program exit with code 0
void generate_program_exit(CodeBuffer* buf) {
    // mov rax, 60  ; sys_exit
    emit_mov_reg_imm64(buf, RAX, 60);
    
    // mov rdi, 0   ; exit code 0
    emit_mov_reg_imm64(buf, RDI, 0);
    
    // syscall
    emit_syscall(buf);
    
    // Add UD2 instruction to trap if we somehow continue
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x0B);
}