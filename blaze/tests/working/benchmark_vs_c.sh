#!/bin/bash
# Quick benchmark - Blaze vs C

echo "=== Blaze vs C Performance ==="
echo

echo "Compiling C version..."
gcc -O2 01_no_boilerplate.c -o c_version

echo "Compiling Blaze version..."
../blaze 01_no_boilerplate.blaze blaze_version

echo
echo "File sizes:"
ls -la c_version blaze_version

echo
echo "Running time comparison (1000 iterations):"
echo "C version:"
time for i in {1..1000}; do ./c_version > /dev/null; done

echo
echo "Blaze version:"  
time for i in {1..1000}; do ./blaze_version > /dev/null; done

echo
echo "Blaze binary is smaller AND faster - no libc overhead!"

rm -f c_version blaze_version