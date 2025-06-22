// Custom startup code for Blaze compiler - Fixed for ALL optimization levels
// Provides _start entry point when not using libc

// Forward declaration of main
int main(int argc, char** argv);

// Ensure these symbols are available (provided by linker)
extern char __bss_start;
extern char _end;

// Critical: Mark _start with specific attributes to prevent optimization issues
__attribute__((naked, noreturn, used, optimize("O0"))) void _start(void) {
    __asm__ volatile(
        // Save initial stack pointer
        "movq %%rsp, %%rbp\n"
        
        // Clear BSS section FIRST (critical for static data!)
        "leaq __bss_start(%%rip), %%rdi\n"
        "leaq _end(%%rip), %%rcx\n"
        "subq %%rdi, %%rcx\n"
        "jz .Lbss_done\n"            // Skip if BSS is empty
        "xorq %%rax, %%rax\n"
        "rep stosb\n"
        ".Lbss_done:\n"
        
        // Memory barrier to ensure BSS clearing completes
        "mfence\n"
        
        // Debug: write "_start\n"
        "movq $1, %%rax\n"          // sys_write
        "movq $1, %%rdi\n"          // stdout
        "leaq .Lmsg1(%%rip), %%rsi\n"
        "movq $8, %%rdx\n"
        "syscall\n"
        
        // Restore stack pointer and prepare for main()
        "movq %%rbp, %%rsp\n"
        
        // Get argc from stack
        "movq (%%rsp), %%rdi\n"     // argc in rdi (first argument)
        "leaq 8(%%rsp), %%rsi\n"    // argv in rsi (second argument)
        
        // Debug: write "calling main\n"
        "pushq %%rdi\n"
        "pushq %%rsi\n"
        "movq $1, %%rax\n"
        "movq $1, %%rdi\n"
        "leaq .Lmsg2(%%rip), %%rsi\n"
        "movq $14, %%rdx\n"
        "syscall\n"
        "popq %%rsi\n"
        "popq %%rdi\n"
        
        // CRITICAL FIX: Setup proper stack frame for optimized code
        // 1. Align stack to 16 bytes
        "andq $-16, %%rsp\n"
        
        // 2. Allocate red zone + local variable space
        // The System V ABI requires 128 bytes of red zone below RSP
        // Plus we need space for main's local variables
        "subq $256, %%rsp\n"         // 128 red zone + 128 for locals
        
        // 3. Push return address slot (required by ABI)
        "pushq $0\n"                 // Dummy return address
        
        // 4. Another memory barrier before calling main
        "mfence\n"
        
        // Call main(argc, argv) - rdi and rsi already set
        "call main\n"
        
        // Exit with return value
        "movl %%eax, %%edi\n"       // exit status = return value
        "movl $60, %%eax\n"         // sys_exit
        "syscall\n"
        
        // Ensure we never return
        "ud2\n"
        
        ".section .rodata\n"
        ".Lmsg1: .ascii \"_start\\n\\0\"\n"
        ".Lmsg2: .ascii \"calling main\\n\\0\"\n"
        ".previous\n"
        
        ::: "rax", "rcx", "rdx", "rsi", "rdi", "rbp", "memory", "cc"
    );
}

// Alternative approach: Provide a small C wrapper that ensures proper setup
void __attribute__((used, optimize("O0"))) _start_helper(int argc, char** argv) {
    // This ensures proper stack frame setup
    int result = main(argc, argv);
    
    // Exit syscall
    __asm__ volatile(
        "movl %0, %%edi\n"
        "movl $60, %%eax\n"
        "syscall\n"
        :: "r"(result)
        : "rdi", "rax"
    );
    __builtin_unreachable();
}