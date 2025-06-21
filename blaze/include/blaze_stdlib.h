// Minimal standard library declarations for Blaze compiler
#ifndef BLAZE_STDLIB_H
#define BLAZE_STDLIB_H

// Basic type definitions
typedef unsigned long size_t;
typedef long ssize_t;
typedef long ptrdiff_t;

// NULL definition
#ifndef NULL
#define NULL ((void*)0)
#endif

// Function declarations
size_t strlen(const char* str);
void* memset(void* dest, int c, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
void abort(void);

// Builtin versions removed - using -fno-builtin

// Stack protection
void __stack_chk_fail(void);
extern long __stack_chk_guard;

// System call wrappers (minimal)
ssize_t write(int fd, const void* buf, size_t count);

#endif // BLAZE_STDLIB_H