// Simplified startup code for debugging optimization issues

int main(int argc, char** argv);

// BSS symbols  
extern char __bss_start;
extern char _end;

__attribute__((naked, noreturn))
void _start(void) {
    __asm__ volatile(
        // Clear BSS
        "leaq __bss_start(%%rip), %%rdi\n"
        "leaq _end(%%rip), %%rcx\n"
        "subq %%rdi, %%rcx\n"
        "jz 1f\n"
        "xorq %%rax, %%rax\n"
        "rep stosb\n"
        "1:\n"
        
        // Get argc and argv
        "movq (%%rsp), %%rdi\n"      // argc
        "leaq 8(%%rsp), %%rsi\n"     // argv
        
        // Align stack before call
        "andq $-16, %%rsp\n"
        "pushq %%rax\n"              // Push something to maintain alignment
        
        // Call main
        "call main\n"
        
        // Exit
        "movl %%eax, %%edi\n"
        "movl $60, %%eax\n"
        "syscall\n"
        ::: "memory"
    );
}