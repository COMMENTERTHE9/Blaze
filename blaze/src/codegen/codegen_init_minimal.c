// MINIMAL CODE GENERATION FOR TESTING
// Just generates a simple exit syscall

#include "blaze_internals.h"

// Forward declarations
extern void emit_mov_reg_imm64(CodeBuffer* buf, X64Register reg, uint64_t value);
extern void emit_syscall(CodeBuffer* buf);

// Generate minimal runtime initialization
void generate_runtime_init_minimal(CodeBuffer* buf) {
    // For Linux x86-64, the kernel starts us with:
    // - RSP points to argc
    // - We should NOT set up a frame because we're not in a function
    // Just leave the stack as-is for now
}