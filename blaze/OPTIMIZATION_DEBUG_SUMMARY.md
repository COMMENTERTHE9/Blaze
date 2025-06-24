# Optimization Level Crash - Debug Summary

## Current Status
The Blaze compiler crashes or produces corrupted output when compiled with optimization levels -O1 and above, despite multiple fixes being applied.

## What Has Been Tried
1. **Volatile declarations** - All static buffers marked as volatile ✓
2. **BSS initialization** - Fixed _start function explicitly clears BSS ✓
3. **Stack alignment** - Proper 16-byte alignment in _start ✓
4. **Memory barriers** - Added mfence instructions ✓
5. **Safe optimization flags** - Tried -fno-strict-aliasing, -fno-aggressive-loop-optimizations ✓

## Test Results
- **-O0**: Works correctly ✅
- **-O1**: Starts but parser gets corrupted values (e.g., name_len=4216978 instead of proper value) ❌
- **-O2/-O3**: Segmentation fault ❌

## Root Cause Analysis
The issue appears to be a fundamental incompatibility between:
1. Large static volatile buffers (463KB total BSS section)
2. Custom _start function with -nostdlib
3. GCC optimizations that don't properly handle this combination

Evidence of corruption in -O1:
```
[PARSER] Created NODE_VAR_DEF at idx=2 name_offset=0 name_len=4216978
[PARSER] Got statement idx=4225281 type=0
[PARSER] Set first_stmt=4227332
```
These are clearly garbage values being read from memory.

## Conclusion
The optimizer is breaking fundamental assumptions about memory access, even with volatile. This suggests:
- Possible GCC bug with -nostdlib and large BSS sections
- Alignment issues that volatile doesn't fix
- Register allocation conflicts in optimized code

## Recommendation
For now, the Blaze compiler should be built with `-O0` only. The performance impact is acceptable given:
1. Compilation is not the bottleneck (code generation is)
2. Reliability is more important than compiler speed
3. The generated machine code is still optimized

## Future Investigation
If optimization is needed later:
1. Consider switching from static buffers to heap allocation
2. Use a proper libc instead of -nostdlib
3. Split the compiler into multiple translation units
4. Investigate using a custom linker script