#!/bin/bash
# Test script to verify -O1 optimization fix

echo "Testing -O1 optimization fix..."

# Clean build with -O1
echo "Building with -O1..."
make clean > /dev/null 2>&1
gcc -O1 -mcmodel=large -fno-section-anchors -fno-pic -Iinclude -o blaze_O1_test \
    src/blaze_compiler_main_clean.c \
    src/lexer/lexer_core.c \
    src/parser/parser_core.c \
    src/parser/symbol_table.c \
    src/parser/symbol_builder.c \
    src/parser/time_travel.c \
    src/codegen/codegen_basic.c \
    src/codegen/codegen_x64.c \
    src/codegen/codegen_x64_sse.c \
    src/codegen/codegen_x64_float_print.c \
    src/codegen/codegen_vars.c \
    src/codegen/codegen_func.c \
    src/codegen/codegen_init_minimal.c \
    src/codegen/codegen_platform.c \
    src/codegen/codegen_output.c \
    src/codegen/elf_generator.c \
    src/codegen/pe_generator.c \
    src/runtime/memory_manager.c \
    src/runtime/blaze_stdlib.c \
    src/runtime/crt0.c \
    -nostdlib -static 2>/dev/null

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

# Test compilation
echo "Compiling test file..."
./blaze_O1_test tests/min.blaze test_out 2>&1 | grep "lex_blaze called with len"

# Check if len=22 is correct
if ./blaze_O1_test tests/min.blaze test_out 2>&1 | grep -q "lex_blaze called with len=22"; then
    echo "✅ PASS: Parameter passing is correct (len=22)"
    echo "✅ -O1 optimization fix verified!"
    exit 0
else
    echo "❌ FAIL: Parameter passing is still corrupted"
    exit 1
fi