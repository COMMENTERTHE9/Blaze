// Custom startup code for Blaze compiler - Fixed for optimization
// Provides _start entry point when not using libc

// Forward declaration of main
int main(int argc, char** argv);

// Ensure these symbols are available (provided by linker)
extern char __bss_start;
extern char _end;

// Entry point for Linux x86-64
__attribute__((naked, noreturn, used, optimize("O0"))) void _start(void) {
    __asm__ volatile(
        // Save initial stack pointer
        "movq %%rsp, %%rbp\n"
        
        // Clear BSS section (critical for optimization!)
        "leaq __bss_start(%%rip), %%rdi\n"
        "leaq _end(%%rip), %%rcx\n"
        "subq %%rdi, %%rcx\n"
        "jz .Lbss_done\n"            // Skip if BSS is empty
        "xorq %%rax, %%rax\n"
        "rep stosb\n"
        ".Lbss_done:\n"
        
        // Memory barrier to ensure BSS clearing completes
        "mfence\n"
        
        
        // Restore stack pointer
        "movq %%rbp, %%rsp\n"
        
        // Get argc from stack
        "movq (%%rsp), %%rdi\n"     // argc in rdi (first argument)
        "leaq 8(%%rsp), %%rsi\n"    // argv in rsi (second argument)
        
        
        // CRITICAL FIX: Proper stack setup for optimized code
        "andq $-16, %%rsp\n"        // Align stack to 16 bytes
        "subq $256, %%rsp\n"        // Allocate red zone + locals space
        "pushq $0\n"                // Push dummy return address
        
        // Another memory barrier before calling main
        "mfence\n"
        
        // Call main(argc, argv)
        "call main\n"
        
        // Exit with return value
        "movl %%eax, %%edi\n"       // exit status = return value
        "movl $60, %%eax\n"         // sys_exit
        "syscall\n"
        
        // Ensure we never return
        "ud2\n"
        
        
        ::: "rax", "rcx", "rdx", "rsi", "rdi", "memory", "cc"
    );
}