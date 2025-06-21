# CLAUDE.md - Blaze Compiler Project Context

## Project Overview
The Blaze compiler translates Blaze language to x86-64 machine code. Currently debugging AST type 243 corruption issue.

## Session Context (2025-01-21)
Fixed -O1 optimization issue by adding proper clobber lists (rcx, r11, memory) to all syscall wrappers. The issue was that print_str() was clobbering RSI which held function parameters.

### Optimization Fix Details
- **Commit**: (pending) "syscall wrappers: declare rcx/r11/memory clobbers"
- **Status**: -O1 verified working, -O2/-O3 need separate investigation
- **Test**: `./test_O1_fix.sh` verifies parameter passing
- **CI**: Added GitHub Actions workflow to prevent regressions

## Critical Syntax Reminders
- **Variables**: `var.v-name-[value]` (NOT `v/ x 5`)
- **Functions**: `|name| verb.can< ... :>`
- **Function calls**: `^name/`
- **Print**: `print/ variable` or `print/ "string"`

## Recent Fixes Applied
1. **Functions** - Fixed AST-based generation (was hardcoded to print "5")
2. **Variables** - Fixed segfault, corrected syntax understanding
3. **Print in functions** - Fixed type 24 error (string pool vs node index)
4. **Function flow** - Added jumps to skip function definitions
5. **-O1 optimization** - Fixed syscall clobber lists in blaze_internals.h and all inline asm
   - Added "rcx", "r11", "memory" to clobber lists
   - Root cause: print_str() was clobbering RSI before printing parameters

## Build Instructions
```bash
# For Sentry-enabled debug build:
make debug
./blaze_debug input.blaze output

# For optimized build (no Sentry):
make
./blaze_compiler input.blaze output
```

## Sentry Integration
- Errors automatically sent to: https://gabriel-it.sentry.io/issues/
- Local log: blaze_errors.log
- Type 243 detection implemented in main compiler

## Current Status
✅ Variables working
✅ Functions working
✅ Print statements working
✅ Sentry error tracking
✅ Type 243 error fixed (was parser bug, now resolved)
✅ -O1 optimization fixed (syscall clobber lists)
🔧 Integrating arithmetic with vars/functions
⚠️ -O2/-O3 still crash on startup (needs investigation)

## Key Files
- `src/codegen/codegen_func.c` - Function generation
- `src/codegen/codegen_vars.c` - Variable handling
- `src/parser/parser_core.c` - Main parser
- `src/blaze_compiler_main.c` - Entry point with Sentry

## Test Commands
```bash
# Test variables
./blaze_debug test_simple_var.blaze out && ./out

# Test integration
./blaze_debug test_integrated_simple.blaze out && ./out

# Force error for Sentry test
echo "bad syntax!!!" > error.blaze
./blaze_debug error.blaze out
```

## Debug Tips
- Parser debug: Look for `[PARSER]` prefixed output
- Symbol table: Look for `[SYMBOL]` prefixed output
- Code generation: Look for `[VAR]`, `[FUNC]`, `[STMT]` prefixes
- Always use `blaze_debug` when testing Sentry integration