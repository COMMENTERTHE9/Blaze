# CLAUDE.md - Blaze Compiler Project Context

## Project Overview
The Blaze compiler translates Blaze language to x86-64 machine code with support for integers, floats, arithmetic operations, and math functions.

## Session Context (2025-01-22)
Working on arithmetic operations with long and float support. Recent commits added:
- Math functions: sin, cos, tan, sqrt, log, exp
- Bitwise operators: &&., ||., ^^, ~~, <<., >>.
- Arithmetic operators including exponentiation (**)
- Float literal support in AST (NODE_FLOAT)

### Optimization Status
- **-O1**: Fixed with proper syscall clobber lists (rcx, r11, memory)
- **-O2/-O3**: Still crash on startup (needs investigation)
- **Test**: `./test_O1_fix.sh` verifies parameter passing

## Critical Syntax Reminders
- **Variables**: `var.v-name-[value]` (NOT `v/ x 5`)
- **Functions**: `|name| verb.can< ... :>`
- **Function calls**: `^name/`
- **Print**: `print/ variable` or `print/ "string"`
- **Math functions**: `math.sin`, `math.cos`, etc.

## Recent Work
1. **Math functions** - Implemented trig, logarithmic, and other math operations
2. **Bitwise operators** - Added conflict-free bitwise ops (&&., ||., etc.)
3. **Arithmetic** - Working on integrating long/float arithmetic with variables
4. **Float support** - AST supports float literals via NODE_FLOAT

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

## Current Status
✅ Variables working
✅ Functions working
✅ Print statements working
✅ Sentry error tracking
✅ -O1 optimization fixed (syscall clobber lists)
✅ Basic arithmetic operators
✅ Math functions (sin, cos, tan, sqrt, log, exp)
🔧 Integrating arithmetic with longs/floats
⚠️ -O2/-O3 still crash on startup

## Key Files
- `src/codegen/codegen_math.c` - Math function generation
- `src/codegen/codegen_x64_sse.c` - SSE/float operations
- `src/codegen/codegen_func.c` - Function generation
- `src/codegen/codegen_vars.c` - Variable handling
- `src/parser/parser_core.c` - Main parser

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