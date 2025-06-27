// Platform-specific print implementations
#include "blaze_internals.h"

// External functions
extern void emit_mov_reg_imm64(CodeBuffer* buf, X64Register reg, uint64_t value);
extern void emit_mov_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_mov_mem_reg(CodeBuffer* buf, X64Register base, int32_t offset, X64Register src);
extern void emit_syscall(CodeBuffer* buf);
extern void emit_byte(CodeBuffer* buf, uint8_t byte);
extern void emit_sub_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_add_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_call_reg(CodeBuffer* buf, X64Register reg);
extern void emit_push_reg(CodeBuffer* buf, X64Register reg);
extern void emit_pop_reg(CodeBuffer* buf, X64Register reg);

// Platform-specific string output
void emit_platform_print_string(CodeBuffer* buf, Platform platform, 
                               const char* str, uint32_t len) {
    switch (platform) {
        case PLATFORM_LINUX:
            // Linux: direct syscall
            emit_mov_reg_imm64(buf, RAX, 1);   // sys_write
            emit_mov_reg_imm64(buf, RDI, 1);   // stdout
            emit_mov_reg_imm64(buf, RSI, (uint64_t)str); // buffer
            emit_mov_reg_imm64(buf, RDX, len); // length
            emit_syscall(buf);
            break;
            
        case PLATFORM_WINDOWS:
            // Windows: Inline WriteFile equivalent
            // For a minimal implementation, we'll use direct console output
            // This is a simplified version that works for basic output
            
            // Save registers Windows ABI requires to be preserved
            emit_push_reg(buf, RBX);
            emit_push_reg(buf, RSI);
            emit_push_reg(buf, RDI);
            
            // Set up for WriteConsoleA call
            // RCX = hConsoleOutput (-11 for stdout)
            emit_mov_reg_imm64(buf, RCX, 0xFFFFFFFFFFFFFFF5); // STD_OUTPUT_HANDLE = -11
            
            // Get console handle via GetStdHandle
            // mov rax, GetStdHandle address (would need IAT)
            // For now, assume handle -11 works directly
            
            // RDX = lpBuffer (string address)
            emit_mov_reg_imm64(buf, RDX, (uint64_t)str);
            
            // R8 = nNumberOfCharsToWrite
            emit_mov_reg_imm64(buf, R8, len);
            
            // R9 = lpNumberOfCharsWritten (NULL)
            emit_mov_reg_imm64(buf, R9, 0);
            
            // For direct console output, we can use INT 0x2E (Windows syscall interface)
            // syscall number for NtWriteFile in RAX
            emit_mov_reg_imm64(buf, RAX, 0x08); // NtWriteFile syscall number
            
            // Set up remaining parameters
            emit_sub_reg_imm32(buf, RSP, 0x28); // Shadow space for Windows x64 ABI
            
            // Do the syscall
            emit_byte(buf, 0x0F);
            emit_byte(buf, 0x05);  // syscall instruction
            
            // Clean up shadow space
            emit_add_reg_imm32(buf, RSP, 0x28);
            
            // Restore registers
            emit_pop_reg(buf, RDI);
            emit_pop_reg(buf, RSI);
            emit_pop_reg(buf, RBX);
            break;
            
        case PLATFORM_MACOS:
            // macOS: BSD-style syscall
            emit_mov_reg_imm64(buf, RAX, 0x2000004); // write
            emit_mov_reg_imm64(buf, RDI, 1);         // stdout
            emit_mov_reg_imm64(buf, RSI, (uint64_t)str);
            emit_mov_reg_imm64(buf, RDX, len);
            emit_syscall(buf);
            break;
    }
}

// Platform-specific character output (for print_int)
void emit_platform_print_char(CodeBuffer* buf, Platform platform) {
    // Assumes character is on stack at [RSP]
    switch (platform) {
        case PLATFORM_LINUX:
            emit_mov_reg_imm64(buf, RAX, 1);  // sys_write
            emit_mov_reg_imm64(buf, RDI, 1);  // stdout
            emit_mov_reg_reg(buf, RSI, RSP);  // address of char
            emit_mov_reg_imm64(buf, RDX, 1);  // length 1
            emit_syscall(buf);
            break;
            
        case PLATFORM_WINDOWS:
            // Save registers
            emit_push_reg(buf, RBX);
            emit_push_reg(buf, RSI);
            emit_push_reg(buf, RDI);
            
            // Set up for console output
            emit_mov_reg_imm64(buf, RCX, 0xFFFFFFFFFFFFFFF5); // stdout handle
            emit_mov_reg_reg(buf, RDX, RSP);  // buffer (after pushes, RSP+24 points to char)
            emit_add_reg_imm32(buf, RDX, 24); // Adjust for pushed registers
            emit_mov_reg_imm64(buf, R8, 1);   // length
            emit_mov_reg_imm64(buf, R9, 0);   // NULL
            
            // Shadow space
            emit_sub_reg_imm32(buf, RSP, 0x28);
            
            // NtWriteFile syscall
            emit_mov_reg_imm64(buf, RAX, 0x08);
            emit_syscall(buf);
            
            // Clean up
            emit_add_reg_imm32(buf, RSP, 0x28);
            
            // Restore registers
            emit_pop_reg(buf, RDI);
            emit_pop_reg(buf, RSI);
            emit_pop_reg(buf, RBX);
            break;
            
        case PLATFORM_MACOS:
            emit_mov_reg_imm64(buf, RAX, 0x2000004); // write
            emit_mov_reg_imm64(buf, RDI, 1);         // stdout
            emit_mov_reg_reg(buf, RSI, RSP);
            emit_mov_reg_imm64(buf, RDX, 1);
            emit_syscall(buf);
            break;
    }
}

// Get current platform from code buffer (needs to be tracked)
Platform get_current_platform(CodeBuffer* buf) {
    // For now, return Linux as default
    // In a full implementation, this would be stored in CodeBuffer
    return PLATFORM_LINUX;
}