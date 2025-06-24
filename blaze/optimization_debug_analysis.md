# Optimization Crash Analysis for Blaze Compiler

## Key Findings

1. **Large Static Arrays in BSS Section**
   - `static char source_buffer[32768];`
   - `static Token tokens[MAX_TOKENS];` (MAX_TOKENS = 4096, Token size likely 16+ bytes = 64KB+)
   - `static ASTNode nodes[4096];` (ASTNode size likely 32+ bytes = 128KB+)
   - `static char string_pool[4096];`
   - `static uint8_t code_buffer[MAX_CODE_SIZE];` (MAX_CODE_SIZE = 65536)
   - `static ExecutionStep execution_plan[1024];`
   - `static SymbolTable symbols;`
   
   Total: ~300KB+ of static data

2. **Naked _start Function with Inline Assembly**
   - Uses `__attribute__((naked))` which disables prologue/epilogue
   - Contains inline assembly with explicit clobber lists
   - Stack alignment handled manually

## Most Likely Causes

### 1. **BSS Section Initialization Issues**
With optimization, the compiler might:
- Reorder BSS initialization
- Assume certain initialization patterns
- Optimize away zero-initialization that's actually needed

### 2. **Stack Alignment Issues**
The naked _start function manually aligns the stack, but optimization might:
- Assume different stack alignment
- Generate SSE instructions that require 16-byte alignment
- Optimize the alignment code itself

### 3. **Inline Assembly Optimization**
With -O0, inline assembly is left alone. With -O1+:
- Register allocation changes
- Memory barriers might be optimized away
- Clobber lists might not be sufficient

### 4. **Relocation/Position-Independent Code Issues**
Static linking with large BSS might cause:
- Relocation overflows
- Issues with RIP-relative addressing
- Problems with the program loader

## Debugging Strategies

### 1. **Immediate Tests**
```bash
# Test with different optimization levels
gcc -nostdlib -static -O1 -o blaze_O1 src/blaze_compiler_main.c ...
gcc -nostdlib -static -O2 -o blaze_O2 src/blaze_compiler_main.c ...
gcc -nostdlib -static -O3 -o blaze_O3 src/blaze_compiler_main.c ...

# Check binary sizes and sections
size blaze_O0 blaze_O1 blaze_O2 blaze_O3
readelf -S blaze_O0 | grep -E "(bss|data)"
readelf -S blaze_O3 | grep -E "(bss|data)"

# Use GDB to see where it crashes
gdb ./blaze_O3
(gdb) break _start
(gdb) run
(gdb) stepi
```

### 2. **Add Memory Barriers**
In _start, add memory barriers:
```c
__asm__ volatile("" ::: "memory");
```

### 3. **Initialize BSS Explicitly**
Add BSS initialization in _start before calling main:
```asm
// Clear BSS section
"leaq __bss_start(%%rip), %%rdi\n"
"leaq _end(%%rip), %%rcx\n"
"subq %%rdi, %%rcx\n"
"xorq %%rax, %%rax\n"
"rep stosb\n"
```

### 4. **Use Compiler Attributes**
For large static arrays:
```c
__attribute__((section(".bss"), aligned(16))) static char source_buffer[32768];
```

### 5. **Test Minimal Optimization Flags**
```bash
# Test specific optimizations
gcc -nostdlib -static -O0 -finline-functions ...
gcc -nostdlib -static -O0 -fomit-frame-pointer ...
gcc -nostdlib -static -O0 -ftree-vectorize ...
```

### 6. **Check Assembly Output**
```bash
# Compare assembly for _start
gcc -S -O0 src/runtime/blaze_start.c -o start_O0.s
gcc -S -O3 src/runtime/blaze_start.c -o start_O3.s
diff -u start_O0.s start_O3.s
```

## Recommended Fixes

1. **Explicit BSS Initialization**
2. **Add volatile to inline assembly**
3. **Use -fno-strict-aliasing with optimization**
4. **Consider using a linker script**
5. **Add __attribute__((used)) to prevent optimization removal**