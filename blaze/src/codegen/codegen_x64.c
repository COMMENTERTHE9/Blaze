// BLAZE X86-64 MACHINE CODE GENERATOR
// Direct emission of x64 instructions with time-travel support

#include "blaze_internals.h"

// Define int8_t if not available
typedef signed char int8_t;

// x86-64 instruction encoding helpers
#define REX_W 0x48
#define REX_WR 0x4C
#define REX_WB 0x49

// MOD/RM byte construction
#define MODRM(mod, reg, rm) (((mod) << 6) | ((reg) << 3) | (rm))

// Emit REX prefix if needed
void emit_rex(CodeBuffer* buf, bool w, bool r, bool x, bool b) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;
    if (r) rex |= 0x04;
    if (x) rex |= 0x02;
    if (b) rex |= 0x01;
    if (rex != 0x40) emit_byte(buf, rex);
}

// Core x64 instructions
void emit_mov_reg_imm64(CodeBuffer* buf, X64Register reg, uint64_t value) {
    emit_rex(buf, true, false, false, reg >= R8);
    emit_byte(buf, 0xB8 + (reg & 7));
    emit_qword(buf, value);
}

void emit_mov_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src) {
    emit_rex(buf, true, src >= R8, false, dst >= R8);
    emit_byte(buf, 0x89);
    emit_byte(buf, MODRM(3, src & 7, dst & 7));
}

void emit_add_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src) {
    emit_rex(buf, true, src >= R8, false, dst >= R8);
    emit_byte(buf, 0x01);
    emit_byte(buf, MODRM(3, src & 7, dst & 7));
}

// ADD instruction with immediate
void emit_add_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value) {
    emit_rex(buf, true, false, false, reg >= R8);
    if (reg == RAX) {
        emit_byte(buf, 0x05);
    } else {
        emit_byte(buf, 0x81);
        emit_byte(buf, MODRM(3, 0, reg & 7));
    }
    emit_dword(buf, value);
}

void emit_sub_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src) {
    emit_rex(buf, true, src >= R8, false, dst >= R8);
    emit_byte(buf, 0x29);
    emit_byte(buf, MODRM(3, src & 7, dst & 7));
}

// SUB instruction with immediate
void emit_sub_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value) {
    emit_rex(buf, true, false, false, reg >= R8);
    if (reg == RAX) {
        emit_byte(buf, 0x2D);
    } else {
        emit_byte(buf, 0x81);
        emit_byte(buf, MODRM(3, 5, reg & 7));
    }
    emit_dword(buf, value);
}

void emit_mul_reg(CodeBuffer* buf, X64Register reg) {
    emit_rex(buf, true, false, false, reg >= R8);
    emit_byte(buf, 0xF7);
    emit_byte(buf, MODRM(3, 4, reg & 7));
}

void emit_div_reg(CodeBuffer* buf, X64Register reg) {
    emit_rex(buf, true, false, false, reg >= R8);
    emit_byte(buf, 0xF7);
    emit_byte(buf, MODRM(3, 6, reg & 7));
}

// Comparison and jumps
void emit_cmp_reg_reg(CodeBuffer* buf, X64Register r1, X64Register r2) {
    emit_rex(buf, true, r2 >= R8, false, r1 >= R8);
    emit_byte(buf, 0x39);
    emit_byte(buf, MODRM(3, r2 & 7, r1 & 7));
}

void emit_cmp_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value) {
    emit_rex(buf, true, false, false, reg >= R8);
    emit_byte(buf, 0x81);
    emit_byte(buf, MODRM(3, 7, reg & 7));
    emit_dword(buf, value);
}

// Jump instructions
void emit_jmp_rel32(CodeBuffer* buf, int32_t offset) {
    emit_byte(buf, 0xE9);
    emit_dword(buf, offset);
}

void emit_je_rel32(CodeBuffer* buf, int32_t offset) {
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x84);
    emit_dword(buf, offset);
}

void emit_jne_rel32(CodeBuffer* buf, int32_t offset) {
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x85);
    emit_dword(buf, offset);
}

void emit_jg_rel32(CodeBuffer* buf, int32_t offset) {
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x8F);
    emit_dword(buf, offset);
}

void emit_jle_rel32(CodeBuffer* buf, int32_t offset) {
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x8E);
    emit_dword(buf, offset);
}

// Stack operations
void emit_push_reg(CodeBuffer* buf, X64Register reg) {
    if (reg >= R8) {
        emit_byte(buf, 0x41);
    }
    emit_byte(buf, 0x50 + (reg & 7));
}

void emit_pop_reg(CodeBuffer* buf, X64Register reg) {
    if (reg >= R8) {
        emit_byte(buf, 0x41);
    }
    emit_byte(buf, 0x58 + (reg & 7));
}

// Memory operations for time-travel state
void emit_mov_mem_reg(CodeBuffer* buf, X64Register base, int32_t offset, X64Register src) {
    emit_rex(buf, true, src >= R8, false, base >= R8);
    emit_byte(buf, 0x89);
    
    if (offset == 0 && base != RBP && base != RSP) {
        emit_byte(buf, MODRM(0, src & 7, base & 7));
    } else if (base == RSP) {
        // RSP requires SIB byte
        if (offset == 0) {
            emit_byte(buf, MODRM(0, src & 7, 4)); // mod=00, SIB follows
            emit_byte(buf, 0x24); // SIB: scale=0, index=none(4), base=RSP(4)
        } else if (offset >= -128 && offset <= 127) {
            emit_byte(buf, MODRM(1, src & 7, 4)); // mod=01, SIB follows
            emit_byte(buf, 0x24); // SIB
            emit_byte(buf, offset);
        } else {
            emit_byte(buf, MODRM(2, src & 7, 4)); // mod=10, SIB follows  
            emit_byte(buf, 0x24); // SIB
            emit_dword(buf, offset);
        }
    } else if (offset >= -128 && offset <= 127) {
        emit_byte(buf, MODRM(1, src & 7, base & 7));
        emit_byte(buf, offset);
    } else {
        emit_byte(buf, MODRM(2, src & 7, base & 7));
        emit_dword(buf, offset);
    }
}

void emit_mov_reg_mem(CodeBuffer* buf, X64Register dst, X64Register base, int32_t offset) {
    emit_rex(buf, true, dst >= R8, false, base >= R8);
    emit_byte(buf, 0x8B);
    
    if (offset == 0 && base != RBP && base != RSP) {
        emit_byte(buf, MODRM(0, dst & 7, base & 7));
    } else if (base == RSP) {
        // RSP requires SIB byte
        if (offset == 0) {
            emit_byte(buf, MODRM(0, dst & 7, 4)); // mod=00, SIB follows
            emit_byte(buf, 0x24); // SIB: scale=0, index=none(4), base=RSP(4)
        } else if (offset >= -128 && offset <= 127) {
            emit_byte(buf, MODRM(1, dst & 7, 4)); // mod=01, SIB follows
            emit_byte(buf, 0x24); // SIB
            emit_byte(buf, offset);
        } else {
            emit_byte(buf, MODRM(2, dst & 7, 4)); // mod=10, SIB follows  
            emit_byte(buf, 0x24); // SIB
            emit_dword(buf, offset);
        }
    } else if (offset >= -128 && offset <= 127) {
        emit_byte(buf, MODRM(1, dst & 7, base & 7));
        emit_byte(buf, offset);
    } else {
        emit_byte(buf, MODRM(2, dst & 7, base & 7));
        emit_dword(buf, offset);
    }
}

// LEA instruction for address calculation
void emit_lea(CodeBuffer* buf, X64Register dst, X64Register base, int32_t offset) {
    emit_rex(buf, true, dst >= R8, false, base >= R8);
    emit_byte(buf, 0x8D);
    
    if (base == RIP) {
        // RIP-relative addressing
        emit_byte(buf, MODRM(0, dst & 7, 5)); // ModRM for RIP-relative
        emit_dword(buf, offset);
    } else if (offset == 0 && base != RBP) {
        emit_byte(buf, MODRM(0, dst & 7, base & 7));
    } else if (offset >= -128 && offset <= 127) {
        emit_byte(buf, MODRM(1, dst & 7, base & 7));
        emit_byte(buf, offset);
    } else {
        emit_byte(buf, MODRM(2, dst & 7, base & 7));
        emit_dword(buf, offset);
    }
}

// System call for output
void emit_syscall(CodeBuffer* buf) {
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x05);
}

// XOR reg, reg (used for zeroing registers)
void emit_xor_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src) {
    emit_rex(buf, true, dst >= R8, false, src >= R8);
    emit_byte(buf, 0x31);
    emit_byte(buf, MODRM(3, src & 7, dst & 7));
}

// INC reg
void emit_inc_reg(CodeBuffer* buf, X64Register reg) {
    emit_rex(buf, true, false, false, reg >= R8);
    emit_byte(buf, 0xFF);
    emit_byte(buf, MODRM(3, 0, reg & 7));
}

// DEC reg
void emit_dec_reg(CodeBuffer* buf, X64Register reg) {
    emit_rex(buf, true, false, false, reg >= R8);
    emit_byte(buf, 0xFF);
    emit_byte(buf, MODRM(3, 1, reg & 7));
}

// MOV [base + index], src - for indexed memory access
void emit_mov_mem_reg_indexed(CodeBuffer* buf, X64Register base, X64Register index, X64Register src) {
    emit_rex(buf, true, src >= R8, index >= R8, base >= R8);
    emit_byte(buf, 0x88); // MOV r/m8, r8
    emit_byte(buf, 0x04 | ((src & 7) << 3)); // ModRM byte
    emit_byte(buf, ((index & 7) << 3) | (base & 7)); // SIB byte
}

// Function prologue/epilogue
void emit_function_prologue(CodeBuffer* buf) {
    emit_push_reg(buf, RBP);
    emit_mov_reg_reg(buf, RBP, RSP);
    // Reserve space for time-travel state (128 bytes)
    emit_rex(buf, true, false, false, false);
    emit_byte(buf, 0x81);
    emit_byte(buf, MODRM(3, 5, RSP));
    emit_dword(buf, 128);
}

void emit_function_epilogue(CodeBuffer* buf) {
    emit_mov_reg_reg(buf, RSP, RBP);
    emit_pop_reg(buf, RBP);
    emit_byte(buf, 0xC3); // RET
}

// Time-travel specific: save/restore temporal state
void emit_save_temporal_state(CodeBuffer* buf, uint8_t marker_id) {
    // Save RAX, RBX, RCX, RDX to temporal storage
    emit_mov_mem_reg(buf, RBP, -16 - (marker_id * 32), RAX);
    emit_mov_mem_reg(buf, RBP, -16 - (marker_id * 32) - 8, RBX);
    emit_mov_mem_reg(buf, RBP, -16 - (marker_id * 32) - 16, RCX);
    emit_mov_mem_reg(buf, RBP, -16 - (marker_id * 32) - 24, RDX);
}

void emit_restore_temporal_state(CodeBuffer* buf, uint8_t marker_id) {
    // Restore RAX, RBX, RCX, RDX from temporal storage
    emit_mov_reg_mem(buf, RAX, RBP, -16 - (marker_id * 32));
    emit_mov_reg_mem(buf, RBX, RBP, -16 - (marker_id * 32) - 8);
    emit_mov_reg_mem(buf, RCX, RBP, -16 - (marker_id * 32) - 16);
    emit_mov_reg_mem(buf, RDX, RBP, -16 - (marker_id * 32) - 24);
}

// Emit conditional with future feedback
void emit_future_conditional(CodeBuffer* buf, TokenType cond_op, X64Register value_reg) {
    // Mark current position for time-travel
    uint32_t temporal_marker = buf->position;
    
    // Emit comparison based on condition type
    switch (cond_op) {
        case TOK_GREATER_THAN:
            emit_cmp_reg_imm32(buf, value_reg, 30);
            emit_jg_rel32(buf, 0); // Placeholder offset
            break;
            
        case TOK_LESS_EQUAL:
            emit_cmp_reg_imm32(buf, value_reg, 30);
            emit_jle_rel32(buf, 0); // Placeholder offset
            break;
            
        case TOK_EQUAL:
            emit_cmp_reg_imm32(buf, value_reg, 0);
            emit_je_rel32(buf, 0); // Placeholder offset
            break;
            
        case TOK_NOT_EQUAL:
            emit_cmp_reg_imm32(buf, value_reg, 0);
            emit_jne_rel32(buf, 0); // Placeholder offset
            break;
    }
    
    // Store temporal marker for later patching
    if (buf->temporal_count < 16) {
        buf->temporal_markers[buf->temporal_count++] = temporal_marker;
    }
}

// GGGX-aware code generation
void emit_gggx_check(CodeBuffer* buf, GGGX_State* gggx) {
    // If provisional (high gap index), emit runtime check
    if (gggx->is_provisional) {
        // Load gap index
        emit_mov_reg_imm64(buf, RAX, gggx->gap_index);
        
        // Compare against threshold (600 = 6.0 scaled)
        emit_cmp_reg_imm32(buf, RAX, 600);
        
        // Jump to safe fallback if gap too high
        emit_jg_rel32(buf, 0); // Will be patched later
    }
    
    // Emit optimized code based on zone prediction
    if (gggx->zone_score < 100) { // Zone (0,1)
        // Aggressive optimization - unroll loops, inline everything
        emit_byte(buf, 0x90); // NOP placeholder for optimization
    } else { // Zone (1,∞)
        // Conservative code generation
        emit_byte(buf, 0x90); // NOP placeholder
    }
}

// TEST reg, reg
void emit_test_reg_reg(CodeBuffer* buf, X64Register reg1, X64Register reg2) {
    emit_rex(buf, true, reg2 >= R8, false, reg1 >= R8);
    emit_byte(buf, 0x85);
    emit_byte(buf, MODRM(3, reg2 & 7, reg1 & 7));
}

// JZ rel8 (jump if zero)
void emit_jz(CodeBuffer* buf, int8_t offset) {
    emit_byte(buf, 0x74);
    emit_byte(buf, offset);
}

// JNZ rel8 (jump if not zero)
void emit_jnz(CodeBuffer* buf, int8_t offset) {
    emit_byte(buf, 0x75);
    emit_byte(buf, offset);
}

// NEG reg (negate register)
void emit_neg_reg(CodeBuffer* buf, X64Register reg) {
    emit_rex(buf, true, false, false, reg >= R8);
    emit_byte(buf, 0xF7);
    emit_byte(buf, MODRM(3, 3, reg & 7));
}

// JGE rel32 (jump if greater or equal)
void emit_jge_rel32(CodeBuffer* buf, int32_t offset) {
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x8D);
    emit_dword(buf, offset);
}

// SHL reg, imm8 (shift left)
void emit_shl_reg_imm8(CodeBuffer* buf, X64Register reg, uint8_t count) {
    emit_rex(buf, true, false, false, reg >= R8);
    if (count == 1) {
        emit_byte(buf, 0xD1);
        emit_byte(buf, MODRM(3, 4, reg & 7));
    } else {
        emit_byte(buf, 0xC1);
        emit_byte(buf, MODRM(3, 4, reg & 7));
        emit_byte(buf, count);
    }
}

// SHR reg, imm8 (shift right logical)
void emit_shr_reg_imm8(CodeBuffer* buf, X64Register reg, uint8_t count) {
    emit_rex(buf, true, false, false, reg >= R8);
    if (count == 1) {
        emit_byte(buf, 0xD1);
        emit_byte(buf, MODRM(3, 5, reg & 7));
    } else {
        emit_byte(buf, 0xC1);
        emit_byte(buf, MODRM(3, 5, reg & 7));
        emit_byte(buf, count);
    }
}

// SAR reg, imm8 (shift right arithmetic)
void emit_sar_reg_imm8(CodeBuffer* buf, X64Register reg, uint8_t count) {
    emit_rex(buf, true, false, false, reg >= R8);
    if (count == 1) {
        emit_byte(buf, 0xD1);
        emit_byte(buf, MODRM(3, 7, reg & 7));
    } else {
        emit_byte(buf, 0xC1);
        emit_byte(buf, MODRM(3, 7, reg & 7));
        emit_byte(buf, count);
    }
}

// IMUL dst, src, imm32 (signed multiply with immediate)
void emit_imul_reg_reg_imm32(CodeBuffer* buf, X64Register dst, X64Register src, int32_t imm) {
    emit_rex(buf, true, dst >= R8, false, src >= R8);
    if (imm >= -128 && imm <= 127) {
        emit_byte(buf, 0x6B);
        emit_byte(buf, MODRM(3, dst & 7, src & 7));
        emit_byte(buf, (int8_t)imm);
    } else {
        emit_byte(buf, 0x69);
        emit_byte(buf, MODRM(3, dst & 7, src & 7));
        emit_dword(buf, imm);
    }
}

// Generate print integer function (for output)
void emit_print_integer(CodeBuffer* buf) {
    // Convert integer in RAX to string and print
    // This is a minimal implementation
    
    // Allocate stack space for string buffer
    emit_rex(buf, true, false, false, false);
    emit_byte(buf, 0x81);
    emit_byte(buf, MODRM(3, 5, RSP));
    emit_dword(buf, 32);
    
    // [Complex conversion code would go here]
    // For now, just print a test message
    
    // sys_write(1, message, length)
    emit_mov_reg_imm64(buf, RAX, 1);    // sys_write
    emit_mov_reg_imm64(buf, RDI, 1);    // stdout
    emit_mov_reg_reg(buf, RSI, RSP);    // buffer
    emit_mov_reg_imm64(buf, RDX, 4);    // length
    emit_syscall(buf);
    
    // Restore stack
    emit_rex(buf, true, false, false, false);
    emit_byte(buf, 0x81);
    emit_byte(buf, MODRM(3, 0, RSP));
    emit_dword(buf, 32);
}