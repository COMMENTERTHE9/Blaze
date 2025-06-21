// Code generation for robust function prologues and epilogues
// Ensures generated code works with ALL optimization levels

#include "blaze_internals.h"

// Forward declarations
extern void emit_byte(CodeBuffer* buf, uint8_t byte);
extern void emit_bytes(CodeBuffer* buf, uint8_t* bytes, uint32_t count);
extern void emit_mov_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_sub_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_add_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_push_reg(CodeBuffer* buf, X64Register reg);
extern void emit_pop_reg(CodeBuffer* buf, X64Register reg);

// Generate entry point that works with optimization
void generate_robust_entry(CodeBuffer* buf) {
    write(1, "Generating robust entry point\n", 30);
    
    // _start label will be at position 0
    // Clear direction flag (required by ABI)
    emit_byte(buf, 0xFC); // cld
    
    // Clear frame pointer
    // xor rbp, rbp
    emit_bytes(buf, (uint8_t[]){0x48, 0x31, 0xED}, 3);
    
    // Save initial stack pointer
    // mov r15, rsp
    emit_bytes(buf, (uint8_t[]){0x49, 0x89, 0xE7}, 3);
    
    // Clear BSS section
    // lea rdi, [rip + __bss_start]
    emit_bytes(buf, (uint8_t[]){0x48, 0x8D, 0x3D}, 3);
    // We'll need to patch this offset later
    uint32_t bss_start_offset_pos = buf->position;
    emit_bytes(buf, (uint8_t[]){0x00, 0x00, 0x00, 0x00}, 4);
    
    // lea rcx, [rip + _end]
    emit_bytes(buf, (uint8_t[]){0x48, 0x8D, 0x0D}, 3);
    uint32_t bss_end_offset_pos = buf->position;
    emit_bytes(buf, (uint8_t[]){0x00, 0x00, 0x00, 0x00}, 4);
    
    // sub rcx, rdi
    emit_bytes(buf, (uint8_t[]){0x48, 0x29, 0xF9}, 3);
    
    // jz .skip_bss_clear
    emit_bytes(buf, (uint8_t[]){0x74, 0x08}, 2);
    
    // xor eax, eax
    emit_bytes(buf, (uint8_t[]){0x31, 0xC0}, 2);
    
    // rep stosb
    emit_bytes(buf, (uint8_t[]){0xF3, 0xAA}, 2);
    
    // mfence - memory barrier
    emit_bytes(buf, (uint8_t[]){0x0F, 0xAE, 0xF0}, 3);
    
    // .skip_bss_clear:
    // Restore stack pointer
    // mov rsp, r15
    emit_bytes(buf, (uint8_t[]){0x4C, 0x89, 0xFC}, 3);
    
    // Get argc and argv
    // mov rdi, [rsp]     ; argc
    emit_bytes(buf, (uint8_t[]){0x48, 0x8B, 0x3C, 0x24}, 4);
    
    // lea rsi, [rsp+8]   ; argv
    emit_bytes(buf, (uint8_t[]){0x48, 0x8D, 0x74, 0x24, 0x08}, 5);
    
    // CRITICAL: Setup proper stack for optimized code
    // and rsp, -16       ; Align to 16 bytes
    emit_bytes(buf, (uint8_t[]){0x48, 0x83, 0xE4, 0xF0}, 4);
    
    // sub rsp, 256       ; Red zone + locals space
    emit_bytes(buf, (uint8_t[]){0x48, 0x81, 0xEC, 0x00, 0x01, 0x00, 0x00}, 7);
    
    // push 0             ; Dummy return address
    emit_bytes(buf, (uint8_t[]){0x6A, 0x00}, 2);
    
    // mfence again before main
    emit_bytes(buf, (uint8_t[]){0x0F, 0xAE, 0xF0}, 3);
    
    // Call offset to main will be patched later
    emit_byte(buf, 0xE8); // call
    uint32_t main_call_offset_pos = buf->position;
    emit_bytes(buf, (uint8_t[]){0x00, 0x00, 0x00, 0x00}, 4);
    
    // Exit with return code
    // mov edi, eax
    emit_bytes(buf, (uint8_t[]){0x89, 0xC7}, 2);
    
    // mov eax, 60  ; sys_exit
    emit_bytes(buf, (uint8_t[]){0xB8, 0x3C, 0x00, 0x00, 0x00}, 5);
    
    // syscall
    emit_bytes(buf, (uint8_t[]){0x0F, 0x05}, 2);
    
    // ud2 - trap if we somehow continue
    emit_bytes(buf, (uint8_t[]){0x0F, 0x0B}, 2);
    
    // Store positions for later patching
    buf->entry_point = 0;
    buf->main_call_offset_pos = main_call_offset_pos;
    buf->bss_offsets_need_patch = true;
}

// Generate function prologue that works with optimization
void generate_function_prologue(CodeBuffer* buf, uint32_t locals_size) {
    // Standard x86-64 prologue
    // push rbp
    emit_push_reg(buf, RBP);
    
    // mov rbp, rsp
    emit_mov_reg_reg(buf, RBP, RSP);
    
    // Ensure we allocate enough space for locals AND respect red zone
    uint32_t total_size = locals_size + 128; // Add red zone
    
    // Align to 16 bytes
    total_size = (total_size + 15) & ~15;
    
    if (total_size > 0) {
        // sub rsp, total_size
        emit_sub_reg_imm32(buf, RSP, total_size);
    }
    
    // Clear allocated stack space (helps with debugging)
    if (total_size > 0 && total_size <= 256) {
        // Small clear using rep stosb
        // mov rdi, rsp
        emit_mov_reg_reg(buf, RDI, RSP);
        // mov rcx, total_size
        emit_bytes(buf, (uint8_t[]){0x48, 0xC7, 0xC1}, 3);
        emit_bytes(buf, (uint8_t*)&total_size, 4);
        // xor eax, eax
        emit_bytes(buf, (uint8_t[]){0x31, 0xC0}, 2);
        // rep stosb
        emit_bytes(buf, (uint8_t[]){0xF3, 0xAA}, 2);
    }
}

// Generate function epilogue
void generate_function_epilogue(CodeBuffer* buf) {
    // mov rsp, rbp
    emit_mov_reg_reg(buf, RSP, RBP);
    
    // pop rbp
    emit_pop_reg(buf, RBP);
    
    // ret
    emit_byte(buf, 0xC3);
}

// Generate main function wrapper with proper setup
void generate_main_wrapper(CodeBuffer* buf, uint32_t user_main_offset) {
    // Generate a main() that sets up environment for user code
    // Save all callee-saved registers
    emit_push_reg(buf, RBP);
    emit_push_reg(buf, RBX);
    emit_push_reg(buf, R12);
    emit_push_reg(buf, R13);
    emit_push_reg(buf, R14);
    emit_push_reg(buf, R15);
    
    // Setup frame
    emit_mov_reg_reg(buf, RBP, RSP);
    
    // Allocate generous stack space
    emit_sub_reg_imm32(buf, RSP, 512);
    
    // Call user's main code
    emit_byte(buf, 0xE8); // call
    int32_t offset = user_main_offset - (buf->position + 4);
    emit_bytes(buf, (uint8_t*)&offset, 4);
    
    // Restore stack
    emit_mov_reg_reg(buf, RSP, RBP);
    
    // Restore registers
    emit_pop_reg(buf, R15);
    emit_pop_reg(buf, R14);
    emit_pop_reg(buf, R13);
    emit_pop_reg(buf, R12);
    emit_pop_reg(buf, RBX);
    emit_pop_reg(buf, RBP);
    
    // Return
    emit_byte(buf, 0xC3);
}