// MINIMAL CODE GENERATION FOR TESTING
// Just generates a simple exit syscall

#include "blaze_internals.h"

// Forward declarations
extern void emit_mov_reg_imm64(CodeBuffer* buf, X64Register reg, uint64_t value);
extern void emit_syscall(CodeBuffer* buf);

// Generate minimal runtime initialization
void generate_runtime_init_minimal(CodeBuffer* buf) {
    // Set up a basic stack frame for the main program
    // This ensures RSP is properly aligned
    extern void emit_push_reg(CodeBuffer* buf, X64Register reg);
    extern void emit_mov_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
    extern void emit_sub_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t imm);
    
    emit_push_reg(buf, RBP);           // push rbp
    emit_mov_reg_reg(buf, RBP, RSP);   // mov rbp, rsp - SET UP FRAME POINTER!
    emit_sub_reg_imm32(buf, RSP, 8);   // align stack to 16 bytes
}