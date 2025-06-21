// Minimal standard library implementation for Blaze compiler
// Provides necessary functions to avoid linking with libc

#include "blaze_internals.h"

// String length - counts bytes until null terminator
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

// Memory set - fills memory with a byte value
void* memset(void* dest, int c, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    while (n--) {
        *d++ = (unsigned char)c;
    }
    return dest;
}

// Memory copy - copies n bytes from src to dest
void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

// Memory move - copies n bytes from src to dest (handles overlap)
void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    if (d < s) {
        // Copy forward
        while (n--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        // Copy backward to handle overlap
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    return dest;
}

// String compare - compares two strings
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

// String compare with length limit
int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

// String copy - copies src to dest
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++) != '\0');
    return dest;
}

// String copy with length limit
char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (n && (*d++ = *src++) != '\0') {
        n--;
    }
    // Pad with zeros if necessary
    while (n--) {
        *d++ = '\0';
    }
    return dest;
}

// Memory compare
int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

// Simple abort implementation
void abort(void) {
    // Exit with error code 134 (SIGABRT)
    __asm__ volatile (
        "mov $60, %%rax\n"  // sys_exit
        "mov $134, %%rdi\n" // exit code
        "syscall\n"
        ::: "rax", "rdi", "rcx", "r11", "memory"
    );
    __builtin_unreachable();
}

// GCC may generate calls to these for struct copies
// Since we use -fno-builtin, these shouldn't be needed, but just in case:

// Stack protection functions (may be needed with some compiler flags)
void __stack_chk_fail(void) {
    // Stack corruption detected
    const char msg[] = "*** stack smashing detected ***\n";
    syscall6(SYS_WRITE, 2, (long)msg, sizeof(msg)-1, 0, 0, 0);
    abort();
}

// This may be referenced by compiler-generated code
long __stack_chk_guard = 0x595959595959;

// System call wrapper for write
ssize_t write(int fd, const void* buf, size_t count) {
    return (ssize_t)syscall6(SYS_WRITE, fd, (long)buf, count, 0, 0, 0);
}