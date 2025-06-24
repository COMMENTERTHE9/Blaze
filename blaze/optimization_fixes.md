# Quick Fixes to Try for Optimization Crashes

## 1. Make Static Arrays Volatile (Easiest)
In `src/blaze_compiler_main.c`, change:
```c
// Global static buffers to avoid stack issues
static char source_buffer[32768];
static Token tokens[MAX_TOKENS];
static ASTNode nodes[4096];
static char string_pool[4096];
static uint8_t code_buffer[MAX_CODE_SIZE];
static ExecutionStep execution_plan[1024];
static SymbolTable symbols;
```

To:
```c
// Global static buffers to avoid stack issues
static volatile char source_buffer[32768];
static volatile Token tokens[MAX_TOKENS];
static volatile ASTNode nodes[4096];
static volatile char string_pool[4096];
static volatile uint8_t code_buffer[MAX_CODE_SIZE];
static volatile ExecutionStep execution_plan[1024];
static volatile SymbolTable symbols;
```

## 2. Add Compiler Flags (Medium)
Update your Makefile to use these flags with optimization:
```makefile
CFLAGS = -O2 -Wall -Wextra -fno-stack-protector -Iinclude \
         -fno-strict-aliasing \
         -fno-aggressive-loop-optimizations \
         -fno-tree-vectorize
```

## 3. Initialize Arrays Explicitly (Better)
Add this function and call it at the start of main():
```c
__attribute__((optimize("O0"))) static void init_static_arrays(void) {
    // Force initialization of first element of each array
    source_buffer[0] = 0;
    tokens[0].type = TOK_EOF;
    nodes[0].type = 0;
    string_pool[0] = 0;
    code_buffer[0] = 0;
    execution_plan[0].node_idx = 0;
    // Force a memory barrier
    __asm__ volatile("" ::: "memory");
}

int main(int argc, char** argv) {
    init_static_arrays();  // Add this line
    write(1, "main() called\n", 14);
    // ... rest of main
}
```

## 4. Use Section Attributes (Advanced)
Force arrays into specific sections:
```c
__attribute__((section(".bss"), aligned(16))) 
static char source_buffer[32768];

__attribute__((section(".bss"), aligned(16))) 
static Token tokens[MAX_TOKENS];
// etc...
```

## 5. Quick Test Commands

Test each fix:
```bash
# With volatile
gcc -nostdlib -static -O3 -Iinclude -o blaze_volatile_test src/blaze_compiler_main.c ... 

# With specific flags
gcc -nostdlib -static -O2 -fno-strict-aliasing -fno-aggressive-loop-optimizations -Iinclude -o blaze_flags_test src/blaze_compiler_main.c ...

# Test
./blaze_volatile_test test.blaze output
```

## Most Likely Solution
The issue is probably that the optimizer assumes BSS is zero-initialized by the OS loader, but with -nostdlib this might not happen. The fixed _start function that explicitly clears BSS should solve this.