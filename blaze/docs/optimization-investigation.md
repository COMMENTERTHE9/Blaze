# Optimization Investigation Log

## Status
- ✅ -O1: Fixed via rcx/r11/memory clobber patches (2025-01-21)
- ❌ -O2: Crashes on startup
- ❌ -O3: Crashes on startup

## -O1 Fix Summary
**Root Cause**: Syscall wrappers weren't declaring clobbered registers (rcx, r11).
**Fix**: Added full clobber lists to all syscall inline asm blocks.
**Files Modified**:
- `include/blaze_internals.h` - syscall6()
- `src/blaze_compiler_main_clean.c` - read_file() syscalls
- `src/runtime/blaze_stdlib.c` - abort() syscall

## -O2/-O3 Investigation

### Symptoms
- Immediate segfault on startup
- Happens before any debug output
- Suggests very early corruption (possibly in _start or crt0)

### Hypotheses
1. More aggressive inlining exposing other missing clobbers
2. Stack alignment issues with custom crt0.c
3. Relocation/addressing issues with large static arrays
4. Undefined behavior being exploited by optimizer

### Test Plan
1. Build with sanitizers: `-O2 -fsanitize=address,undefined`
2. Try selective disabling: `-O2 -fno-inline-functions`
3. Check assembly output: `-O2 -S -fverbose-asm`
4. Test with standard libc to isolate nostdlib issues

### Debug Commands
```bash
# Build with AddressSanitizer
gcc -O2 -g -fsanitize=address -mcmodel=large -fno-section-anchors -fno-pic \
    -Iinclude -o blaze_O2_asan [sources...] -static-libasan

# Run with ASan options
ASAN_OPTIONS=halt_on_error=1,print_stats=1 ./blaze_O2_asan tests/min.blaze out

# Generate assembly listing
gcc -O2 -S -fverbose-asm -mcmodel=large -fno-section-anchors -fno-pic \
    -Iinclude src/blaze_compiler_main_clean.c -o main_O2.s
```

### Findings
(To be updated as investigation progresses)