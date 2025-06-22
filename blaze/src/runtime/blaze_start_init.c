// Initialization function called by _start
// This is in a separate file to avoid optimization issues

#include <stdint.h>

// Forward declaration of main
int main(int argc, char** argv);

// BSS symbols
extern char __bss_start;
extern char _end;

// Exit system call
static void exit(int status) {
    __asm__ volatile (
        "mov $60, %%rax\n"  // sys_exit
        "mov %0, %%edi\n"   // exit status
        "syscall\n"
        :: "r"(status)
        : "rax", "rdi"
    );
    __builtin_unreachable();
}

// Debug write function
static void debug_write(const char* msg) {
    int len = 0;
    while (msg[len]) len++;
    
    __asm__ volatile (
        "mov $1, %%rax\n"   // sys_write
        "mov $1, %%rdi\n"   // stdout
        "mov %0, %%rsi\n"   // message
        "mov %1, %%edx\n"   // length
        "syscall\n"
        :: "r"(msg), "r"(len)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11"
    );
}

// This function handles the actual initialization
__attribute__((noinline, optimize("O0")))
void _start_init(int argc, char** argv) {
    debug_write("_start_init entered\n");
    
    // Debug print argc
    debug_write("argc = ");
    char num[16];
    int i = 0;
    int n = argc;
    if (n == 0) {
        num[i++] = '0';
    } else {
        while (n > 0) {
            num[i++] = '0' + (n % 10);
            n /= 10;
        }
    }
    num[i++] = '\n';
    num[i] = '\0';
    debug_write(num);
    
    debug_write("About to call main\n");
    
    // Call main
    int ret = main(argc, argv);
    
    debug_write("main returned\n");
    
    // Exit with return value
    exit(ret);
}