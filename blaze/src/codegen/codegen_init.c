// CODE GENERATION FOR RUNTIME INITIALIZATION
// Generates initialization code that runs before main program

#include "blaze_internals.h"

// Forward declarations
extern void emit_mov_reg_imm64(CodeBuffer* buf, X64Register reg, uint64_t value);
extern void emit_mov_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_mov_mem_reg(CodeBuffer* buf, X64Register base, int32_t offset, X64Register src);
extern void emit_mov_reg_mem(CodeBuffer* buf, X64Register dst, X64Register base, int32_t offset);
extern void emit_cmp_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_syscall(CodeBuffer* buf);
extern void emit_call_reg(CodeBuffer* buf, X64Register reg);
extern void emit_byte(CodeBuffer* buf, uint8_t byte);

// Generate runtime initialization code
void generate_runtime_init(CodeBuffer* buf) {
    // Instead of calling external memory_init, generate inline initialization
    // This makes the generated executable standalone
    
    // Map memory regions using mmap syscall
    // mmap(addr, length, prot, flags, fd, offset)
    // prot = PROT_READ|PROT_WRITE = 3
    // flags = MAP_PRIVATE|MAP_ANONYMOUS = 0x22 (no MAP_FIXED)
    
    // Map arena (let kernel choose address)
    emit_mov_reg_imm64(buf, RAX, 9);        // mmap syscall
    emit_mov_reg_imm64(buf, RDI, 0);        // addr (0 = kernel chooses)
    emit_mov_reg_imm64(buf, RSI, 0x600000); // length (6MB)
    emit_mov_reg_imm64(buf, RDX, 3);        // prot
    emit_mov_reg_imm64(buf, R10, 0x22);     // flags (no MAP_FIXED)
    emit_mov_reg_imm64(buf, R8, -1);        // fd
    emit_mov_reg_imm64(buf, R9, 0);         // offset
    emit_syscall(buf);
    
    // Initialize arena header at 0x100000
    emit_mov_reg_imm64(buf, RAX, 0x100000);
    emit_mov_reg_imm64(buf, RCX, 16);       // sizeof(ArenaHeader)
    emit_mov_mem_reg(buf, RAX, 0, RCX);     // current_offset = 16
    emit_mov_reg_imm64(buf, RCX, 0x600000);
    emit_mov_mem_reg(buf, RAX, 8, RCX);     // arena_size = 6MB
    emit_mov_reg_imm64(buf, RCX, 16);
    emit_mov_mem_reg(buf, RAX, 16, RCX);    // reset_point = 16
    emit_mov_reg_imm64(buf, RCX, 0);
    emit_mov_mem_reg(buf, RAX, 24, RCX);    // action_depth = 0
    
    // Map temporal zones at 0x700000
    emit_mov_reg_imm64(buf, RAX, 9);        // mmap syscall
    emit_mov_reg_imm64(buf, RDI, 0);        // addr (0 = kernel chooses)
    emit_mov_reg_imm64(buf, RSI, 0x300000); // length (3MB)
    emit_mov_reg_imm64(buf, RDX, 3);        // prot
    emit_mov_reg_imm64(buf, R10, 0x22);     // flags (no MAP_FIXED)
    emit_mov_reg_imm64(buf, R8, -1);        // fd
    emit_mov_reg_imm64(buf, R9, 0);         // offset
    emit_syscall(buf);
    
    // Map heap at 0xA00000
    emit_mov_reg_imm64(buf, RAX, 9);        // mmap syscall
    emit_mov_reg_imm64(buf, RDI, 0);        // addr (0 = kernel chooses)
    emit_mov_reg_imm64(buf, RSI, 0x1600000);// length (22MB)
    emit_mov_reg_imm64(buf, RDX, 3);        // prot
    emit_mov_reg_imm64(buf, R10, 0x22);     // flags (no MAP_FIXED)
    emit_mov_reg_imm64(buf, R8, -1);        // fd
    emit_mov_reg_imm64(buf, R9, 0);         // offset
    emit_syscall(buf);
    
    // Store heap_current pointer at a fixed location (0xA00000 - 8)
    emit_mov_reg_imm64(buf, RAX, 0xA00000 - 8);
    emit_mov_reg_imm64(buf, RCX, 0xA00000);
    emit_mov_mem_reg(buf, RAX, 0, RCX);     // heap_current = 0xA00000
}

// Generate code to allocate memory using arena allocator
void generate_arena_alloc(CodeBuffer* buf, X64Register size_reg, X64Register result_reg) {
    // Inline arena allocation
    // Load arena header pointer
    emit_mov_reg_imm64(buf, RAX, 0x100000);
    
    // Align size to 16 bytes: size = (size + 15) & ~15
    emit_mov_reg_reg(buf, RCX, size_reg);
    emit_mov_reg_imm64(buf, RDX, 15);
    emit_byte(buf, 0x48); emit_byte(buf, 0x01); emit_byte(buf, 0xD1); // ADD RCX, RDX
    emit_mov_reg_imm64(buf, RDX, ~15ULL);
    emit_byte(buf, 0x48); emit_byte(buf, 0x21); emit_byte(buf, 0xD1); // AND RCX, RDX
    
    // Load current offset
    emit_mov_reg_mem(buf, RDX, RAX, 0);
    
    // Calculate new offset = current + size
    emit_mov_reg_reg(buf, R8, RDX);
    emit_byte(buf, 0x49); emit_byte(buf, 0x01); emit_byte(buf, 0xC8); // ADD R8, RCX
    
    // Check if new offset > arena_size
    emit_mov_reg_mem(buf, R9, RAX, 8);
    emit_byte(buf, 0x4D); emit_byte(buf, 0x39); emit_byte(buf, 0xC8); // CMP R8, R9
    
    // Jump if above (allocation would overflow)
    emit_byte(buf, 0x77); emit_byte(buf, 0x15); // JA +21 bytes
    
    // Update current offset
    emit_mov_mem_reg(buf, RAX, 0, R8);
    
    // Calculate return address = ARENA_START + old_offset
    emit_mov_reg_imm64(buf, result_reg, 0x100000);
    // ADD result_reg, RDX
    if (result_reg >= R8) {
        emit_byte(buf, 0x49); // REX.WB
    } else {
        emit_byte(buf, 0x48); // REX.W
    }
    emit_byte(buf, 0x01);
    emit_byte(buf, 0xD0 | (result_reg & 7)); // ModRM
    
    // Jump to end
    emit_byte(buf, 0xEB); emit_byte(buf, 0x07); // JMP +7
    
    // Allocation failed - return NULL
    emit_mov_reg_imm64(buf, result_reg, 0);
}

// Generate code to allocate reference-counted memory
void generate_rc_alloc(CodeBuffer* buf, X64Register size_reg, X64Register result_reg) {
    // Inline RC allocation
    // Load heap_current from fixed location
    emit_mov_reg_imm64(buf, RAX, 0xA00000 - 8);
    emit_mov_reg_mem(buf, RDX, RAX, 0); // RDX = heap_current
    
    // Calculate total size = sizeof(RCHeader) + size
    emit_mov_reg_reg(buf, RCX, size_reg);
    emit_mov_reg_imm64(buf, R8, 8); // sizeof(RCHeader) = 8
    emit_byte(buf, 0x49); emit_byte(buf, 0x01); emit_byte(buf, 0xC8); // ADD R8, RCX
    
    // Align to 16 bytes
    emit_mov_reg_imm64(buf, R9, 15);
    emit_byte(buf, 0x4D); emit_byte(buf, 0x01); emit_byte(buf, 0xC8); // ADD R8, R9
    emit_mov_reg_imm64(buf, R9, ~15ULL);
    emit_byte(buf, 0x4D); emit_byte(buf, 0x21); emit_byte(buf, 0xC8); // AND R8, R9
    
    // Check if allocation fits in heap
    emit_mov_reg_reg(buf, R9, RDX);
    emit_byte(buf, 0x4D); emit_byte(buf, 0x01); emit_byte(buf, 0xC1); // ADD R9, R8
    emit_mov_reg_imm64(buf, R10, 0xA00000 + 0x1600000); // heap end
    emit_byte(buf, 0x4D); emit_byte(buf, 0x39); emit_byte(buf, 0xD1); // CMP R9, R10
    
    // Jump if above (would overflow)
    emit_byte(buf, 0x77); emit_byte(buf, 0x25); // JA +37 bytes
    
    // Initialize RCHeader
    emit_mov_mem_reg(buf, RDX, 0, RCX);     // size = requested size
    emit_mov_reg_imm64(buf, RCX, 1);
    emit_byte(buf, 0x66); emit_byte(buf, 0x89); emit_byte(buf, 0x4A); emit_byte(buf, 0x04); // MOV [RDX+4], CX (refcount=1)
    emit_mov_reg_imm64(buf, RCX, 0);
    emit_byte(buf, 0x66); emit_byte(buf, 0x89); emit_byte(buf, 0x4A); emit_byte(buf, 0x06); // MOV [RDX+6], CX (flags=0)
    
    // Update heap_current
    emit_mov_mem_reg(buf, RAX, 0, R9);
    
    // Return pointer after header
    emit_mov_reg_reg(buf, result_reg, RDX);
    emit_mov_reg_imm64(buf, RCX, 8);
    // ADD result_reg, RCX
    if (result_reg >= R8) {
        emit_byte(buf, 0x49); // REX.WB
    } else {
        emit_byte(buf, 0x48); // REX.W
    }
    emit_byte(buf, 0x01);
    emit_byte(buf, 0xC8 | (result_reg & 7)); // ModRM
    
    // Jump to end
    emit_byte(buf, 0xEB); emit_byte(buf, 0x07); // JMP +7
    
    // Allocation failed - return NULL
    emit_mov_reg_imm64(buf, result_reg, 0);
}

// Generate code to allocate temporal memory
void generate_temporal_alloc(CodeBuffer* buf, TimeZone zone, X64Register size_reg, X64Register result_reg) {
    // For now, temporal allocation just uses rc_alloc
    // TODO: Implement proper temporal zone management
    generate_rc_alloc(buf, size_reg, result_reg);
}

// Generate code to enter an action block
void generate_arena_enter_action(CodeBuffer* buf) {
    // Inline arena_enter_action
    // Load arena header
    emit_mov_reg_imm64(buf, RAX, 0x100000);
    
    // Increment action_depth
    emit_mov_reg_mem(buf, RCX, RAX, 24);
    emit_mov_reg_imm64(buf, RDX, 1);
    emit_byte(buf, 0x48); emit_byte(buf, 0x01); emit_byte(buf, 0xD1); // ADD RCX, RDX
    emit_mov_mem_reg(buf, RAX, 24, RCX);
    
    // If action_depth == 1, save reset point
    emit_cmp_reg_imm32(buf, RCX, 1);
    emit_byte(buf, 0x75); emit_byte(buf, 0x08); // JNE +8
    
    // Save current offset as reset point
    emit_mov_reg_mem(buf, RDX, RAX, 0);
    emit_mov_mem_reg(buf, RAX, 16, RDX);
}

// Generate code to exit an action block
void generate_arena_exit_action(CodeBuffer* buf) {
    // Inline arena_exit_action
    // Load arena header
    emit_mov_reg_imm64(buf, RAX, 0x100000);
    
    // Load action_depth
    emit_mov_reg_mem(buf, RCX, RAX, 24);
    
    // If action_depth > 0, decrement it
    emit_cmp_reg_imm32(buf, RCX, 0);
    emit_byte(buf, 0x74); emit_byte(buf, 0x18); // JE +24 (skip if 0)
    
    // Decrement action_depth
    emit_mov_reg_imm64(buf, RDX, 1);
    emit_byte(buf, 0x48); emit_byte(buf, 0x29); emit_byte(buf, 0xD1); // SUB RCX, RDX
    emit_mov_mem_reg(buf, RAX, 24, RCX);
    
    // If action_depth == 0, reset arena
    emit_cmp_reg_imm32(buf, RCX, 0);
    emit_byte(buf, 0x75); emit_byte(buf, 0x08); // JNE +8
    
    // Reset current offset to reset point
    emit_mov_reg_mem(buf, RDX, RAX, 16);
    emit_mov_mem_reg(buf, RAX, 0, RDX);
}

// Generate code to increase reference count
void generate_rc_inc(CodeBuffer* buf, X64Register ptr_reg) {
    // Move pointer to RDI (first argument)
    if (ptr_reg != RDI) {
        emit_mov_reg_reg(buf, RDI, ptr_reg);
    }
    
    // Call rc_inc
    emit_mov_reg_imm64(buf, RAX, (uint64_t)rc_inc);
    emit_byte(buf, 0xFF);
    emit_byte(buf, 0xD0); // CALL RAX
}

// Generate code to decrease reference count
void generate_rc_dec(CodeBuffer* buf, X64Register ptr_reg) {
    // Move pointer to RDI (first argument)
    if (ptr_reg != RDI) {
        emit_mov_reg_reg(buf, RDI, ptr_reg);
    }
    
    // Call rc_dec
    emit_mov_reg_imm64(buf, RAX, (uint64_t)rc_dec);
    emit_byte(buf, 0xFF);
    emit_byte(buf, 0xD0); // CALL RAX
}