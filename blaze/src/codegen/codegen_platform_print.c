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
            
        case PLATFORM_WINDOWS: {
            // Windows: Use WriteConsoleA via import table
            // The IAT is at fixed RVA 0x2060
            // GetStdHandle is at [0x140002060]
            // WriteConsoleA is at [0x140002068]
            
            // Save registers we'll use
            emit_push_reg(buf, RCX);
            emit_push_reg(buf, RDX);
            emit_push_reg(buf, R8);
            emit_push_reg(buf, R9);
            emit_push_reg(buf, R10);
            emit_push_reg(buf, R11);
            
            // First call GetStdHandle to get console handle
            emit_mov_reg_imm64(buf, RCX, -11); // STD_OUTPUT_HANDLE
            emit_sub_reg_imm32(buf, RSP, 0x28); // Shadow space + alignment
            
            // Call GetStdHandle via IAT
            // mov rax, [rip + offset_to_iat]
            emit_byte(buf, 0x48); // REX.W
            emit_byte(buf, 0x8B); // MOV
            emit_byte(buf, 0x05); // ModRM for RAX, [RIP+disp32]
            
            // Calculate offset to IAT entry for GetStdHandle
            // IAT is at RVA 0x2060, we're at RVA 0x1000 + current position
            int32_t get_std_offset = 0x2060 - (0x1000 + buf->position + 4);
            emit_byte(buf, get_std_offset & 0xFF);
            emit_byte(buf, (get_std_offset >> 8) & 0xFF);
            emit_byte(buf, (get_std_offset >> 16) & 0xFF);
            emit_byte(buf, (get_std_offset >> 24) & 0xFF);
            
            // call rax
            emit_byte(buf, 0xFF);
            emit_byte(buf, 0xD0);
            
            // Save console handle
            emit_mov_reg_reg(buf, R10, RAX);
            
            // Now call WriteConsoleA
            // Set up parameters
            emit_mov_reg_reg(buf, RCX, R10);      // hConsole
            emit_mov_reg_imm64(buf, RDX, (uint64_t)str); // lpBuffer
            emit_mov_reg_imm64(buf, R8, len);     // nNumberOfCharsToWrite
            emit_mov_reg_reg(buf, R9, RSP);       // lpNumberOfCharsWritten (use stack)
            
            // Call WriteConsoleA via IAT
            // mov rax, [rip + offset_to_iat]
            emit_byte(buf, 0x48); // REX.W
            emit_byte(buf, 0x8B); // MOV
            emit_byte(buf, 0x05); // ModRM for RAX, [RIP+disp32]
            
            // WriteConsoleA is 8 bytes after GetStdHandle in IAT
            int32_t write_offset = 0x2068 - (0x1000 + buf->position + 4);
            emit_byte(buf, write_offset & 0xFF);
            emit_byte(buf, (write_offset >> 8) & 0xFF);
            emit_byte(buf, (write_offset >> 16) & 0xFF);
            emit_byte(buf, (write_offset >> 24) & 0xFF);
            
            // call rax
            emit_byte(buf, 0xFF);
            emit_byte(buf, 0xD0);
            
            // Clean up shadow space
            emit_add_reg_imm32(buf, RSP, 0x28);
            
            // Restore registers
            emit_pop_reg(buf, R11);
            emit_pop_reg(buf, R10);
            emit_pop_reg(buf, R9);
            emit_pop_reg(buf, R8);
            emit_pop_reg(buf, RDX);
            emit_pop_reg(buf, RCX);
            break;
        }
            
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
            
        case PLATFORM_WINDOWS: {
            // Windows: Use WriteConsoleA for single character
            // Save registers
            emit_push_reg(buf, RCX);
            emit_push_reg(buf, RDX);
            emit_push_reg(buf, R8);
            emit_push_reg(buf, R9);
            emit_push_reg(buf, R10);
            emit_push_reg(buf, R11);
            
            // Get console handle
            emit_mov_reg_imm64(buf, RCX, -11); // STD_OUTPUT_HANDLE
            emit_sub_reg_imm32(buf, RSP, 0x28); // Shadow space
            
            // Call GetStdHandle
            emit_byte(buf, 0x48); emit_byte(buf, 0x8B); emit_byte(buf, 0x05);
            int32_t get_offset = 0x2060 - (0x1000 + buf->position + 4);
            emit_byte(buf, get_offset & 0xFF);
            emit_byte(buf, (get_offset >> 8) & 0xFF);
            emit_byte(buf, (get_offset >> 16) & 0xFF);
            emit_byte(buf, (get_offset >> 24) & 0xFF);
            emit_byte(buf, 0xFF); emit_byte(buf, 0xD0); // call rax
            
            // Save handle
            emit_mov_reg_reg(buf, R10, RAX);
            
            // Call WriteConsoleA with character
            emit_mov_reg_reg(buf, RCX, R10);    // hConsole
            emit_lea(buf, RDX, RSP, 0x50);      // lpBuffer (char is above our pushes)
            emit_mov_reg_imm64(buf, R8, 1);     // nNumberOfCharsToWrite = 1
            emit_mov_reg_reg(buf, R9, RSP);     // lpNumberOfCharsWritten
            
            // Call WriteConsoleA
            emit_byte(buf, 0x48); emit_byte(buf, 0x8B); emit_byte(buf, 0x05);
            int32_t write_offset = 0x2068 - (0x1000 + buf->position + 4);
            emit_byte(buf, write_offset & 0xFF);
            emit_byte(buf, (write_offset >> 8) & 0xFF);
            emit_byte(buf, (write_offset >> 16) & 0xFF);
            emit_byte(buf, (write_offset >> 24) & 0xFF);
            emit_byte(buf, 0xFF); emit_byte(buf, 0xD0); // call rax
            
            // Clean up
            emit_add_reg_imm32(buf, RSP, 0x28);
            
            // Restore registers
            emit_pop_reg(buf, R11);
            emit_pop_reg(buf, R10);
            emit_pop_reg(buf, R9);
            emit_pop_reg(buf, R8);
            emit_pop_reg(buf, RDX);
            emit_pop_reg(buf, RCX);
            break;
        }
            
        case PLATFORM_MACOS:
            emit_mov_reg_imm64(buf, RAX, 0x2000004); // write
            emit_mov_reg_imm64(buf, RDI, 1);         // stdout
            emit_mov_reg_reg(buf, RSI, RSP);
            emit_mov_reg_imm64(buf, RDX, 1);
            emit_syscall(buf);
            break;
    }
}

// Platform-specific runtime string output
// String pointer in RSI, length in RDX
void emit_platform_print_string_runtime(CodeBuffer* buf, Platform platform) {
    switch (platform) {
        case PLATFORM_LINUX:
            // Linux: direct syscall
            emit_mov_reg_imm64(buf, RAX, 1);   // sys_write
            emit_mov_reg_imm64(buf, RDI, 1);   // stdout
            // RSI already has string pointer
            // RDX already has length
            emit_syscall(buf);
            break;
            
        case PLATFORM_WINDOWS: {
            // Windows: Use WriteConsoleA via import table
            // String is in RSI, length is in RDX
            
            // Save registers
            emit_push_reg(buf, RCX);
            emit_push_reg(buf, RDX);
            emit_push_reg(buf, R8);
            emit_push_reg(buf, R9);
            emit_push_reg(buf, R10);
            emit_push_reg(buf, RSI); // Save string pointer
            
            // Get console handle
            emit_mov_reg_imm64(buf, RCX, -11); // STD_OUTPUT_HANDLE
            emit_sub_reg_imm32(buf, RSP, 0x28); // Shadow space
            
            // Call GetStdHandle
            emit_byte(buf, 0x48); emit_byte(buf, 0x8B); emit_byte(buf, 0x05);
            int32_t get_offset = 0x2060 - (0x1000 + buf->position + 4);
            emit_byte(buf, get_offset & 0xFF);
            emit_byte(buf, (get_offset >> 8) & 0xFF);
            emit_byte(buf, (get_offset >> 16) & 0xFF);
            emit_byte(buf, (get_offset >> 24) & 0xFF);
            emit_byte(buf, 0xFF); emit_byte(buf, 0xD0); // call rax
            
            // Save handle
            emit_mov_reg_reg(buf, R10, RAX);
            
            // Restore string pointer and length
            emit_add_reg_imm32(buf, RSP, 0x28); // Remove shadow space
            emit_pop_reg(buf, RSI); // Restore string pointer
            
            // Set up WriteConsoleA parameters
            emit_mov_reg_reg(buf, RCX, R10);      // hConsole
            emit_mov_reg_reg(buf, RDX, RSI);      // lpBuffer
            emit_mov_reg_reg(buf, R8, RDX);       // nNumberOfCharsToWrite (was in RDX)
            emit_mov_reg_reg(buf, R9, RSP);       // lpNumberOfCharsWritten (use stack)
            
            // Re-allocate shadow space for WriteConsoleA
            emit_sub_reg_imm32(buf, RSP, 0x28);
            
            // Call WriteConsoleA
            emit_byte(buf, 0x48); emit_byte(buf, 0x8B); emit_byte(buf, 0x05);
            int32_t write_offset = 0x2068 - (0x1000 + buf->position + 4);
            emit_byte(buf, write_offset & 0xFF);
            emit_byte(buf, (write_offset >> 8) & 0xFF);
            emit_byte(buf, (write_offset >> 16) & 0xFF);
            emit_byte(buf, (write_offset >> 24) & 0xFF);
            emit_byte(buf, 0xFF); emit_byte(buf, 0xD0); // call rax
            
            // Clean up
            emit_add_reg_imm32(buf, RSP, 0x28);
            
            // Restore registers
            emit_pop_reg(buf, R10);
            emit_pop_reg(buf, R9);
            emit_pop_reg(buf, R8);
            emit_pop_reg(buf, RDX);
            emit_pop_reg(buf, RCX);
            break;
        }
            
        case PLATFORM_MACOS:
            // macOS: BSD-style syscall
            emit_mov_reg_imm64(buf, RAX, 0x2000004); // write
            emit_mov_reg_imm64(buf, RDI, 1);         // stdout
            // RSI already has string pointer
            // RDX already has length
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