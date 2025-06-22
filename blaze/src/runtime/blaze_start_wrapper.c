// Wrapper approach for startup code
// Separates naked _start from C wrapper

// Forward declaration of main
int main(int argc, char** argv);

// BSS symbols
extern char __bss_start;
extern char _end;

// Write helper
static void write_msg(const char* msg, int len) {
    __asm__ volatile (
        "movq $1, %%rax\n"
        "movq $1, %%rdi\n"
        "movq %0, %%rsi\n"
        "movq %1, %%rdx\n"
        "syscall\n"
        :
        : "r"(msg), "r"((long)len)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11"
    );
}

// C wrapper function
__attribute__((used, noinline, optimize("O0"))) int _start_wrapper(int argc, char** argv) {
    write_msg("wrapper called\n", 15);
    
    // Ensure stack is properly set up
    volatile char stack_guard[256] = {0};
    (void)stack_guard;
    
    // Call main
    int ret = main(argc, argv);
    
    // Exit
    __asm__ volatile (
        "movl %0, %%edi\n"
        "movl $60, %%eax\n"
        "syscall\n"
        :: "r"(ret)
        : "rdi", "rax"
    );
    
    return 0; // Never reached
}

// Naked entry point
__attribute__((naked, noreturn)) void _start(void) {
    __asm__ volatile(
        // Clear BSS
        "leaq __bss_start(%%rip), %%rdi\n"
        "leaq _end(%%rip), %%rcx\n"
        "subq %%rdi, %%rcx\n"
        "jz .Lbss_done2\n"
        "xorq %%rax, %%rax\n"
        "rep stosb\n"
        ".Lbss_done2:\n"
        
        // Get argc and argv
        "movq (%%rsp), %%rdi\n"
        "leaq 8(%%rsp), %%rsi\n"
        
        // Setup stack properly
        "andq $-16, %%rsp\n"
        "subq $256, %%rsp\n"    // Red zone + locals
        "pushq $0\n"            // Dummy return address
        
        // Call wrapper
        "call _start_wrapper\n"
        
        // Should never reach here
        "ud2\n"
        ::: "memory"
    );
}