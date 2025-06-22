// crt0.c - Custom startup with proper BSS clearing
// Compile with -O0 -fno-stack-protector

extern char __bss_start[], _end[];
extern int main(int argc, char** argv);

__attribute__((naked, noreturn))
void _start(void) {
    __asm__ volatile(
        // Save initial stack pointer
        "movq %%rsp, %%rbp\n"
        
        // Clear BSS manually - critical!
        "leaq __bss_start(%%rip), %%rdi\n"
        "leaq _end(%%rip), %%rcx\n"
        "subq %%rdi, %%rcx\n"        // rcx = size of BSS
        "jz .Lbss_done\n"             // Skip if BSS is empty
        
        // Clear byte by byte
        ".Lclear_loop:\n"
        "movb $0, (%%rdi)\n"
        "incq %%rdi\n"
        "decq %%rcx\n"
        "jnz .Lclear_loop\n"
        
        ".Lbss_done:\n"
        "mfence\n"                    // Memory barrier
        
        // Restore stack pointer
        "movq %%rbp, %%rsp\n"
        
        // Get argc and argv
        "movq (%%rsp), %%rdi\n"       // argc in rdi
        "leaq 8(%%rsp), %%rsi\n"     // argv in rsi
        
        // Align stack to 16 bytes
        "andq $-16, %%rsp\n"
        "subq $8, %%rsp\n"            // Misalign for call
        
        // Call main(argc, argv)
        "call main\n"
        
        // Exit with return value
        "movl %%eax, %%edi\n"         // exit status = return value
        "movl $60, %%eax\n"           // sys_exit
        "syscall\n"
        
        // Should never reach here
        "ud2\n"
        ::: "memory"
    );
}