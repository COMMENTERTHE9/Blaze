#!/bin/bash

echo "=== Running Solid Number Tests ==="
echo

# Build directory
BUILD_DIR="../build"
mkdir -p $BUILD_DIR

# Compile tests
echo "Compiling tests..."
gcc -o $BUILD_DIR/test_lexer_solid test_lexer_solid_numbers.c ../src/lexer/lexer_core.c -I../include -g
if [ $? -ne 0 ]; then
    echo "Failed to compile lexer tests"
    exit 1
fi

gcc -o $BUILD_DIR/test_solid_errors ../test_solid_lexer_errors.c ../src/lexer/lexer_core.c -I../include -g
if [ $? -ne 0 ]; then
    echo "Failed to compile error handling tests"
    exit 1
fi

# Run tests
echo
echo "Running lexer unit tests..."
$BUILD_DIR/test_lexer_solid
LEXER_RESULT=$?

echo
echo "Running error handling tests..."
$BUILD_DIR/test_solid_errors > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "Error handling tests: PASSED"
    ERROR_RESULT=0
else
    echo "Error handling tests: FAILED"
    ERROR_RESULT=1
fi

# Summary
echo
echo "=== TEST SUMMARY ==="
if [ $LEXER_RESULT -eq 0 ] && [ $ERROR_RESULT -eq 0 ]; then
    echo "All solid number tests PASSED!"
    exit 0
else
    echo "Some tests FAILED!"
    exit 1
fi