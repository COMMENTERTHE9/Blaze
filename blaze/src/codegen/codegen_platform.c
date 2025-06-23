// PLATFORM-SPECIFIC CODE GENERATION
// Handles differences between Windows and Linux

#include "blaze_internals.h"

// Platform is defined in blaze_internals.h

// System call numbers
typedef struct {
    uint32_t exit;
    uint32_t write;
    uint32_t read;
    uint32_t open;
    uint32_t close;
    uint32_t mmap;
} SyscallNumbers;

// Platform-specific syscall tables
static const SyscallNumbers linux_syscalls = {
    .exit = 60,
    .write = 1,
    .read = 0,
    .open = 2,
    .close = 3,
    .mmap = 9
};

// Windows doesn't use syscall numbers directly
static const SyscallNumbers windows_syscalls = {
    .exit = 0,    // Will use different mechanism
    .write = 0,
    .read = 0,
    .open = 0,
    .close = 0,
    .mmap = 0
};

// Generate platform-specific exit code
void emit_platform_exit(CodeBuffer* buf, Platform platform, int exit_code) {
    switch (platform) {
        case PLATFORM_LINUX:
            // Ensure stack is 16-byte aligned for exit syscall
            // and rsp, -16
            emit_byte(buf, 0x48);
            emit_byte(buf, 0x83);
            emit_byte(buf, 0xE4);
            emit_byte(buf, 0xF0);
            
            // Linux x64 exit syscall
            emit_mov_reg_imm64(buf, RAX, 60);        // sys_exit
            emit_mov_reg_imm64(buf, RDI, exit_code); // exit code
            emit_syscall(buf);
            break;
            
        case PLATFORM_WINDOWS:
            // Windows x64 - different calling convention
            // rcx = exit code (first parameter)
            emit_mov_reg_imm64(buf, RCX, exit_code);
            
            // For minimal PE, just return
            // In full implementation, would call ExitProcess
            emit_byte(buf, 0xC3); // RET
            break;
            
        case PLATFORM_MACOS:
            // macOS x64 exit syscall (BSD-style)
            emit_mov_reg_imm64(buf, RAX, 0x2000001); // exit
            emit_mov_reg_imm64(buf, RDI, exit_code);
            emit_syscall(buf);
            break;
    }
}

// Generate platform-specific write to stdout
void emit_platform_write_stdout(CodeBuffer* buf, Platform platform, 
                               X64Register data_reg, X64Register len_reg) {
    switch (platform) {
        case PLATFORM_LINUX:
            // Linux write syscall
            emit_mov_reg_imm64(buf, RAX, 1);   // sys_write
            emit_mov_reg_imm64(buf, RDI, 1);   // stdout
            emit_mov_reg_reg(buf, RSI, data_reg); // buffer
            emit_mov_reg_reg(buf, RDX, len_reg);  // length
            emit_syscall(buf);
            break;
            
        case PLATFORM_WINDOWS:
            // Windows WriteFile via kernel32.dll
            // This is more complex - would need IAT
            // For now, emit a placeholder
            emit_byte(buf, 0x90); // NOP
            break;
            
        case PLATFORM_MACOS:
            // macOS write syscall
            emit_mov_reg_imm64(buf, RAX, 0x2000004); // write
            emit_mov_reg_imm64(buf, RDI, 1);         // stdout
            emit_mov_reg_reg(buf, RSI, data_reg);
            emit_mov_reg_reg(buf, RDX, len_reg);
            emit_syscall(buf);
            break;
    }
}

// Generate platform-specific memory allocation
void emit_platform_alloc(CodeBuffer* buf, Platform platform,
                        X64Register size_reg, X64Register result_reg) {
    switch (platform) {
        case PLATFORM_LINUX:
            // Linux mmap for anonymous memory
            emit_mov_reg_imm64(buf, RAX, 9);         // sys_mmap
            emit_mov_reg_imm64(buf, RDI, 0);         // addr = NULL
            emit_mov_reg_reg(buf, RSI, size_reg);    // length
            emit_mov_reg_imm64(buf, RDX, 3);         // PROT_READ|PROT_WRITE
            emit_mov_reg_imm64(buf, R10, 0x22);      // MAP_PRIVATE|MAP_ANONYMOUS
            emit_mov_reg_imm64(buf, R8, -1);         // fd = -1
            emit_mov_reg_imm64(buf, R9, 0);          // offset = 0
            emit_syscall(buf);
            emit_mov_reg_reg(buf, result_reg, RAX);
            break;
            
        case PLATFORM_WINDOWS:
            // Windows VirtualAlloc
            // Would need to call through IAT
            emit_byte(buf, 0x90); // NOP placeholder
            break;
            
        case PLATFORM_MACOS:
            // macOS mmap
            emit_mov_reg_imm64(buf, RAX, 0x20000C5); // mmap
            emit_mov_reg_imm64(buf, RDI, 0);
            emit_mov_reg_reg(buf, RSI, size_reg);
            emit_mov_reg_imm64(buf, RDX, 3);
            emit_mov_reg_imm64(buf, RCX, 0x1002);    // MAP_PRIVATE|MAP_ANON
            emit_mov_reg_imm64(buf, R8, -1);
            emit_mov_reg_imm64(buf, R9, 0);
            emit_syscall(buf);
            emit_mov_reg_reg(buf, result_reg, RAX);
            break;
    }
}

// Adjust calling convention for platform
void emit_platform_function_call(CodeBuffer* buf, Platform platform,
                                uint64_t func_addr, X64Register args[6], int arg_count) {
    switch (platform) {
        case PLATFORM_LINUX:
        case PLATFORM_MACOS:
            // System V AMD64 ABI
            // Arguments: RDI, RSI, RDX, RCX, R8, R9
            if (arg_count > 0) emit_mov_reg_reg(buf, RDI, args[0]);
            if (arg_count > 1) emit_mov_reg_reg(buf, RSI, args[1]);
            if (arg_count > 2) emit_mov_reg_reg(buf, RDX, args[2]);
            if (arg_count > 3) emit_mov_reg_reg(buf, RCX, args[3]);
            if (arg_count > 4) emit_mov_reg_reg(buf, R8,  args[4]);
            if (arg_count > 5) emit_mov_reg_reg(buf, R9,  args[5]);
            break;
            
        case PLATFORM_WINDOWS:
            // Microsoft x64 calling convention
            // Arguments: RCX, RDX, R8, R9 (plus shadow space)
            
            // Allocate shadow space (32 bytes)
            emit_byte(buf, 0x48); emit_byte(buf, 0x83); 
            emit_byte(buf, 0xEC); emit_byte(buf, 0x20);
            
            if (arg_count > 0) emit_mov_reg_reg(buf, RCX, args[0]);
            if (arg_count > 1) emit_mov_reg_reg(buf, RDX, args[1]);
            if (arg_count > 2) emit_mov_reg_reg(buf, R8,  args[2]);
            if (arg_count > 3) emit_mov_reg_reg(buf, R9,  args[3]);
            // Args 5+ go on stack
            break;
    }
    
    // Call the function
    emit_mov_reg_imm64(buf, RAX, func_addr);
    emit_byte(buf, 0xFF);
    emit_byte(buf, 0xD0); // CALL RAX
    
    if (platform == PLATFORM_WINDOWS) {
        // Clean up shadow space
        emit_byte(buf, 0x48); emit_byte(buf, 0x83);
        emit_byte(buf, 0xC4); emit_byte(buf, 0x20);
    }
}

// Get default platform based on compilation target
Platform get_default_platform() {
#ifdef _WIN32
    return PLATFORM_WINDOWS;
#elif defined(__APPLE__)
    return PLATFORM_MACOS;
#else
    return PLATFORM_LINUX;
#endif
}

// Platform-specific startup code
void emit_platform_startup(CodeBuffer* buf, Platform platform) {
    switch (platform) {
        case PLATFORM_LINUX:
            // Linux: entry point is _start, need to set up stack
            // Already done by kernel
            break;
            
        case PLATFORM_WINDOWS:
            // Windows: entry point gets RCX = module handle
            // Save it if needed
            emit_push_reg(buf, RCX);
            break;
            
        case PLATFORM_MACOS:
            // macOS: similar to Linux
            break;
    }
}

// Get platform name
const char* get_platform_name(Platform platform) {
    switch (platform) {
        case PLATFORM_LINUX:   return "Linux";
        case PLATFORM_WINDOWS: return "Windows";
        case PLATFORM_MACOS:   return "macOS";
        default:               return "Unknown";
    }
}

// Check if platform supports direct syscalls
bool platform_has_syscalls(Platform platform) {
    return platform == PLATFORM_LINUX || platform == PLATFORM_MACOS;
}

// Generate platform-specific time query (for time-travel runtime)
void emit_platform_get_time(CodeBuffer* buf, Platform platform, X64Register result_reg) {
    switch (platform) {
        case PLATFORM_LINUX:
            // clock_gettime(CLOCK_MONOTONIC, &timespec)
            emit_mov_reg_imm64(buf, RAX, 228);    // sys_clock_gettime
            emit_mov_reg_imm64(buf, RDI, 1);      // CLOCK_MONOTONIC
            emit_mov_reg_reg(buf, RSI, RSP);      // Use stack for timespec
            emit_byte(buf, 0x48); emit_byte(buf, 0x83); 
            emit_byte(buf, 0xEC); emit_byte(buf, 0x10); // Allocate 16 bytes
            emit_syscall(buf);
            emit_mov_reg_mem(buf, result_reg, RSP, 0); // Load seconds
            emit_byte(buf, 0x48); emit_byte(buf, 0x83);
            emit_byte(buf, 0xC4); emit_byte(buf, 0x10); // Restore stack
            break;
            
        case PLATFORM_WINDOWS:
            // QueryPerformanceCounter
            // Would need Windows API
            emit_mov_reg_imm64(buf, result_reg, 0);
            break;
            
        case PLATFORM_MACOS:
            // mach_absolute_time()
            emit_byte(buf, 0x0F); emit_byte(buf, 0x31); // RDTSC
            emit_mov_reg_reg(buf, result_reg, RAX);
            break;
    }
}