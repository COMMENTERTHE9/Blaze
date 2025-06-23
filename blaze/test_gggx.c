#include "include/blaze_internals.h"
#include "include/solid_runtime.h"
#include "include/gggx.h"

// For va_list
typedef __builtin_va_list va_list;
#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap, type) __builtin_va_arg(ap, type)

// Simple sprintf implementation for testing
int sprintf(char* buf, const char* fmt, ...) {
    // Very basic implementation - for our use case, we need to handle:
    // %.6f for doubles, %u for unsigned int, %s for strings, %.1f for doubles
    va_list args;
    va_start(args, fmt);
    
    int len = 0;
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == '.') {
                fmt++; // Skip '.'
                while (*fmt >= '0' && *fmt <= '9') fmt++; // Skip precision
            }
            
            if (*fmt == 'f') {
                // Double argument
                double val = va_arg(args, double);
                int int_part = (int)val;
                
                // Convert integer part
                if (int_part == 0) {
                    buf[len++] = '0';
                } else {
                    char temp[32];
                    int temp_len = 0;
                    int neg = 0;
                    if (int_part < 0) {
                        neg = 1;
                        int_part = -int_part;
                    }
                    while (int_part > 0) {
                        temp[temp_len++] = '0' + (int_part % 10);
                        int_part /= 10;
                    }
                    if (neg) buf[len++] = '-';
                    for (int i = temp_len - 1; i >= 0; i--) {
                        buf[len++] = temp[i];
                    }
                }
                
                // Add decimal part
                buf[len++] = '.';
                double frac = val - (int)val;
                if (frac < 0) frac = -frac;
                for (int i = 0; i < 6; i++) {
                    frac *= 10;
                    int digit = (int)frac;
                    buf[len++] = '0' + digit;
                    frac -= digit;
                }
            } else if (*fmt == 'u') {
                // Unsigned int
                unsigned int val = va_arg(args, unsigned int);
                if (val == 0) {
                    buf[len++] = '0';
                } else {
                    char temp[32];
                    int temp_len = 0;
                    while (val > 0) {
                        temp[temp_len++] = '0' + (val % 10);
                        val /= 10;
                    }
                    for (int i = temp_len - 1; i >= 0; i--) {
                        buf[len++] = temp[i];
                    }
                }
            } else if (*fmt == 's') {
                // String
                const char* str = va_arg(args, const char*);
                while (*str) {
                    buf[len++] = *str++;
                }
            }
            fmt++;
        } else {
            buf[len++] = *fmt++;
        }
    }
    
    buf[len] = '\0';
    va_end(args);
    return len;
}

// Natural log approximation
double log(double x) {
    // Very rough approximation for testing
    if (x <= 0) return -1000000;
    if (x == 1) return 0;
    
    // Use series expansion around x=1
    double result = 0;
    double term = (x - 1);
    double power = term;
    
    for (int n = 1; n < 10; n++) {
        result += power / n;
        power *= -term;
    }
    
    return result;
}

int main() {
    print_str("=== GGGX ALGORITHM TEST ===\n");
    
    // Initialize solid number pool
    solid_pool_init();
    
    // Test 1: Simple integer
    print_str("\nTest 1: Simple integer (42)\n");
    GGGXResult* result1 = gggx_analyze(42.0, 10);
    gggx_print_result(result1);
    
    // Test 2: Simple fraction (22/7 ≈ π)
    print_str("\nTest 2: Simple fraction (22/7)\n");
    GGGXResult* result2 = gggx_analyze(3.142857142857, 15);
    gggx_print_result(result2);
    
    // Test 3: Mathematical constant (π)
    print_str("\nTest 3: Mathematical constant (π)\n");
    GGGXResult* result3 = gggx_analyze(3.14159265359, 20);
    gggx_print_result(result3);
    
    // Test 4: Repeating decimal (1/3)
    print_str("\nTest 4: Repeating decimal (1/3)\n");
    GGGXResult* result4 = gggx_analyze(0.333333333333, 12);
    gggx_print_result(result4);
    
    // Test 5: Irrational number (√2)
    print_str("\nTest 5: Irrational number (√2)\n");
    GGGXResult* result5 = gggx_analyze(1.41421356237, 15);
    gggx_print_result(result5);
    
    // Test 6: Very small number
    print_str("\nTest 6: Very small number\n");
    GGGXResult* result6 = gggx_analyze(0.000000123456, 10);
    gggx_print_result(result6);
    
    // Test 7: Negative number
    print_str("\nTest 7: Negative number\n");
    GGGXResult* result7 = gggx_analyze(-2.71828182846, 12);
    gggx_print_result(result7);
    
    // Clean up
    gggx_free_result(result1);
    gggx_free_result(result2);
    gggx_free_result(result3);
    gggx_free_result(result4);
    gggx_free_result(result5);
    gggx_free_result(result6);
    gggx_free_result(result7);
    
    print_str("\n=== TEST COMPLETE ===\n");
    return 0;
}