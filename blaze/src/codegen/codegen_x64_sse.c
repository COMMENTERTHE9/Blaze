// SSE/SSE2 floating-point instruction support for Blaze
// Handles double-precision (64-bit) floating point operations

#include "blaze_internals.h"



// Forward declarations for x64 instruction emitters


// Forward declarations for x64 instruction emitters
void emit_mov_reg_imm64(CodeBuffer* buf, X64Register reg, uint64_t value);
void emit_push_reg(CodeBuffer* buf, X64Register reg);
void emit_add_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
void emit_movsd_xmm_mem(CodeBuffer* buf, SSERegister dst, X64Register base); // Forward declaration

// Forward declarations
void emit_byte(CodeBuffer* buf, uint8_t byte);
void emit_dword(CodeBuffer* buf, uint32_t value);
void emit_qword(CodeBuffer* buf, uint64_t value);
void emit_rex(CodeBuffer* buf, bool w, bool r, bool x, bool b);

#define MODRM(mod, reg, rm) (((mod) << 6) | ((reg) << 3) | (rm))

// Load double immediate into XMM register
// This is complex - we need to store the constant in memory and load it
void emit_movsd_xmm_imm(CodeBuffer* buf, SSERegister reg, double value) {
    // For now, use a simpler approach: load via general purpose register
    // This avoids RIP-relative addressing issues
    
    // MOV RAX, immediate_value (as 64-bit integer)
    uint64_t* double_bits = (uint64_t*)&value;
    emit_mov_reg_imm64(buf, RAX, *double_bits);
    
    // PUSH RAX (store on stack)
    emit_push_reg(buf, RAX);
    
    // MOVSD xmm, [RSP] (load from stack)
    emit_movsd_xmm_mem(buf, reg, RSP);
    
    // ADD RSP, 8 (clean up stack)
    emit_add_reg_imm32(buf, RSP, 8);
}

// MOVSD xmm1, xmm2 - Move scalar double
void emit_movsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src) {
    emit_byte(buf, 0xF2); // SD prefix
    if (dst >= XMM8 || src >= XMM8) {
        emit_rex(buf, false, dst >= XMM8, false, src >= XMM8);
    }
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x10);
    emit_byte(buf, MODRM(3, dst & 7, src & 7));
}

// MOVSD xmm, [reg] - Load scalar double from memory
void emit_movsd_xmm_mem(CodeBuffer* buf, SSERegister dst, X64Register base) {
    emit_byte(buf, 0xF2); // SD prefix
    if (dst >= XMM8 || base >= R8) {
        emit_rex(buf, false, dst >= XMM8, false, base >= R8);
    }
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x10);
    
    // Special handling for RSP
    if ((base & 7) == 4) {
        emit_byte(buf, MODRM(0, dst & 7, 4)); // 4 = SIB follows
        emit_byte(buf, 0x24); // SIB: scale=0, index=4 (none), base=4 (RSP)
    } else {
        emit_byte(buf, MODRM(0, dst & 7, base & 7));
    }
}

// MOVSD [reg], xmm - Store scalar double to memory
void emit_movsd_mem_xmm(CodeBuffer* buf, X64Register base, SSERegister src) {
    emit_byte(buf, 0xF2); // SD prefix
    if (src >= XMM8 || base >= R8) {
        emit_rex(buf, false, src >= XMM8, false, base >= R8);
    }
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x11);
    
    // Special handling for RSP
    if ((base & 7) == 4) {
        emit_byte(buf, MODRM(0, src & 7, 4)); // 4 = SIB follows
        emit_byte(buf, 0x24); // SIB: scale=0, index=4 (none), base=4 (RSP)
    } else {
        emit_byte(buf, MODRM(0, src & 7, base & 7));
    }
}

// ADDSD xmm1, xmm2 - Add scalar double
void emit_addsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src) {
    emit_byte(buf, 0xF2); // SD prefix
    if (dst >= XMM8 || src >= XMM8) {
        emit_rex(buf, false, dst >= XMM8, false, src >= XMM8);
    }
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x58);
    emit_byte(buf, MODRM(3, dst & 7, src & 7));
}

// SUBSD xmm1, xmm2 - Subtract scalar double
void emit_subsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src) {
    emit_byte(buf, 0xF2); // SD prefix
    if (dst >= XMM8 || src >= XMM8) {
        emit_rex(buf, false, dst >= XMM8, false, src >= XMM8);
    }
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x5C);
    emit_byte(buf, MODRM(3, dst & 7, src & 7));
}

// MULSD xmm1, xmm2 - Multiply scalar double
void emit_mulsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src) {
    emit_byte(buf, 0xF2); // SD prefix
    if (dst >= XMM8 || src >= XMM8) {
        emit_rex(buf, false, dst >= XMM8, false, src >= XMM8);
    }
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x59);
    emit_byte(buf, MODRM(3, dst & 7, src & 7));
}

// DIVSD xmm1, xmm2 - Divide scalar double
void emit_divsd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src) {
    emit_byte(buf, 0xF2); // SD prefix
    if (dst >= XMM8 || src >= XMM8) {
        emit_rex(buf, false, dst >= XMM8, false, src >= XMM8);
    }
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x5E);
    emit_byte(buf, MODRM(3, dst & 7, src & 7));
}

// UCOMISD xmm1, xmm2 - Compare scalar doubles (sets flags)
void emit_ucomisd_xmm_xmm(CodeBuffer* buf, SSERegister dst, SSERegister src) {
    emit_byte(buf, 0x66); // 66 prefix for UCOMISD
    if (dst >= XMM8 || src >= XMM8) {
        emit_rex(buf, false, dst >= XMM8, false, src >= XMM8);
    }
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x2E);
    emit_byte(buf, MODRM(3, dst & 7, src & 7));
}

// CVTSI2SD xmm, reg - Convert integer to scalar double
void emit_cvtsi2sd_xmm_reg(CodeBuffer* buf, SSERegister dst, X64Register src) {
    emit_byte(buf, 0xF2); // SD prefix
    emit_rex(buf, true, dst >= XMM8, false, src >= R8); // REX.W for 64-bit
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x2A);
    emit_byte(buf, MODRM(3, dst & 7, src & 7));
}

// CVTSD2SI reg, xmm - Convert scalar double to integer
void emit_cvtsd2si_reg_xmm(CodeBuffer* buf, X64Register dst, SSERegister src) {
    emit_byte(buf, 0xF2); // SD prefix
    emit_rex(buf, true, dst >= R8, false, src >= XMM8); // REX.W for 64-bit
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x2D);
    emit_byte(buf, MODRM(3, dst & 7, src & 7));
}

// COMISD xmm1, xmm2 - Compare scalar double (sets flags)
void emit_comisd_xmm_xmm(CodeBuffer* buf, SSERegister xmm1, SSERegister xmm2) {
    emit_byte(buf, 0x66); // 66 prefix for COMISD
    if (xmm1 >= XMM8 || xmm2 >= XMM8) {
        emit_rex(buf, false, xmm1 >= XMM8, false, xmm2 >= XMM8);
    }
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x2F);
    emit_byte(buf, MODRM(3, xmm1 & 7, xmm2 & 7));
}