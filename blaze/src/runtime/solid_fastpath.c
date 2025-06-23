// SOLID NUMBER FAST PATH OPTIMIZATIONS
// Specialized fast paths for common operations

#include "blaze_internals.h"
#include "solid_runtime.h"

// Cache for small exact integers (-128 to 127)
static SolidNumber* small_int_cache[256];
static bool cache_initialized = false;

// Initialize small integer cache
static void init_small_int_cache(void) {
    if (cache_initialized) return;
    
    print_str("[SOLID-FAST] Initializing small integer cache...\n");
    
    for (int i = -128; i < 128; i++) {
        char buffer[8];
        int len = 0;
        int val = i;
        
        if (val < 0) {
            buffer[len++] = '-';
            val = -val;
        }
        
        if (val == 0) {
            buffer[len++] = '0';
        } else {
            char temp[4];
            int temp_len = 0;
            while (val > 0) {
                temp[temp_len++] = '0' + (val % 10);
                val /= 10;
            }
            for (int j = temp_len - 1; j >= 0; j--) {
                buffer[len++] = temp[j];
            }
        }
        
        small_int_cache[i + 128] = solid_init_exact(buffer, len);
        solid_inc_ref(small_int_cache[i + 128]);  // Keep in cache
    }
    
    cache_initialized = true;
}

// Fast path for small integer creation
SolidNumber* solid_from_int_fast(int64_t value) {
    // Check small integer cache
    if (value >= -128 && value <= 127) {
        if (!cache_initialized) init_small_int_cache();
        SolidNumber* cached = small_int_cache[value + 128];
        solid_inc_ref(cached);
        return cached;
    }
    
    // Fast path for other integers
    char buffer[32];
    int len = 0;
    int64_t val = value;
    
    if (val < 0) {
        buffer[len++] = '-';
        val = -val;
    }
    
    // Unrolled digit conversion for speed
    if (val == 0) {
        buffer[len++] = '0';
    } else {
        char temp[20];
        int temp_len = 0;
        
        // Unroll by processing 2 digits at a time
        while (val >= 100) {
            int two_digits = val % 100;
            val /= 100;
            temp[temp_len++] = '0' + (two_digits % 10);
            temp[temp_len++] = '0' + (two_digits / 10);
        }
        
        if (val >= 10) {
            temp[temp_len++] = '0' + (val % 10);
            temp[temp_len++] = '0' + (val / 10);
        } else if (val > 0) {
            temp[temp_len++] = '0' + val;
        }
        
        // Reverse
        for (int i = temp_len - 1; i >= 0; i--) {
            buffer[len++] = temp[i];
        }
    }
    
    return solid_init_exact(buffer, len);
}

// Fast path for exact integer addition
SolidNumber* solid_add_int_fast(SolidNumber* a, int64_t b) {
    // Quick check if a is exact
    if (a->barrier_type != BARRIER_EXACT) {
        // Fall back to regular path
        SolidNumber* b_solid = solid_from_int_fast(b);
        SolidNumber* result = solid_add(a, b_solid);
        solid_dec_ref(b_solid);
        return result;
    }
    
    // Fast conversion of a to integer
    const char* digits = solid_get_known_digits(a);
    int64_t val_a = 0;
    int sign = 1;
    int i = 0;
    
    if (digits[0] == '-') {
        sign = -1;
        i = 1;
    }
    
    // Fast digit parsing
    while (i < a->known_len && digits[i] != '.') {
        val_a = val_a * 10 + (digits[i] - '0');
        i++;
    }
    val_a *= sign;
    
    // Perform addition
    int64_t result = val_a + b;
    
    // Check for overflow to use exact path
    if ((b > 0 && result < val_a) || (b < 0 && result > val_a)) {
        // Overflow - use regular path
        SolidNumber* b_solid = solid_from_int_fast(b);
        SolidNumber* res = solid_add(a, b_solid);
        solid_dec_ref(b_solid);
        return res;
    }
    
    return solid_from_int_fast(result);
}

// Fast path for multiplication by power of 10
SolidNumber* solid_multiply_pow10_fast(SolidNumber* a, int power) {
    if (power == 0) {
        solid_inc_ref(a);
        return a;
    }
    
    // For exact numbers, just shift decimal point
    if (a->barrier_type == BARRIER_EXACT) {
        const char* digits = solid_get_known_digits(a);
        char result[256];
        int len = 0;
        
        // Find decimal point
        int decimal_pos = -1;
        for (int i = 0; i < a->known_len; i++) {
            if (digits[i] == '.') {
                decimal_pos = i;
                break;
            }
        }
        
        if (power > 0) {
            // Multiply by 10^power - shift left
            if (decimal_pos == -1) {
                // No decimal - just add zeros
                for (int i = 0; i < a->known_len; i++) {
                    result[len++] = digits[i];
                }
                for (int i = 0; i < power; i++) {
                    result[len++] = '0';
                }
            } else {
                // Has decimal - move it right
                for (int i = 0; i < a->known_len; i++) {
                    if (i != decimal_pos) {
                        result[len++] = digits[i];
                    }
                }
                
                // Add zeros if needed
                int digits_after_decimal = a->known_len - decimal_pos - 1;
                if (power > digits_after_decimal) {
                    for (int i = 0; i < power - digits_after_decimal; i++) {
                        result[len++] = '0';
                    }
                }
            }
        } else {
            // Divide by 10^(-power) - shift right
            power = -power;
            
            // Copy and insert decimal at appropriate position
            int num_digits = decimal_pos == -1 ? a->known_len : decimal_pos;
            
            if (power >= num_digits) {
                // Result is 0.00...
                result[len++] = '0';
                result[len++] = '.';
                for (int i = 0; i < power - num_digits; i++) {
                    result[len++] = '0';
                }
                for (int i = 0; i < a->known_len; i++) {
                    if (digits[i] != '.') {
                        result[len++] = digits[i];
                    }
                }
            } else {
                // Insert decimal in middle
                int new_decimal_pos = num_digits - power;
                for (int i = 0; i < a->known_len; i++) {
                    if (i == new_decimal_pos && decimal_pos == -1) {
                        result[len++] = '.';
                    }
                    if (digits[i] != '.') {
                        result[len++] = digits[i];
                    }
                }
            }
        }
        
        return solid_init_exact(result, len);
    }
    
    // For non-exact, adjust gap magnitude
    uint64_t new_gap = a->gap_magnitude;
    if (power > 0) {
        for (int i = 0; i < power && new_gap < ~0ULL / 10; i++) {
            new_gap *= 10;
        }
    } else {
        for (int i = 0; i < -power && new_gap > 1; i++) {
            new_gap /= 10;
        }
    }
    
    return solid_init_with_gap(solid_get_known_digits(a), a->known_len,
                              a->barrier_type, new_gap,
                              a->confidence_x1000,
                              solid_get_terminal_digits(a), a->terminal_len,
                              a->terminal_type);
}

// Fast path for comparison with zero
bool solid_is_zero_fast(SolidNumber* s) {
    if (!s || s->barrier_type != BARRIER_EXACT) {
        return false;
    }
    
    const char* digits = solid_get_known_digits(s);
    
    // Fast check for common cases
    if (s->known_len == 1 && digits[0] == '0') {
        return true;
    }
    
    if (s->known_len == 2 && digits[0] == '-' && digits[1] == '0') {
        return true;  // "-0"
    }
    
    // Check all significant digits
    bool found_nonzero = false;
    for (uint16_t i = 0; i < s->known_len; i++) {
        if (digits[i] >= '1' && digits[i] <= '9') {
            found_nonzero = true;
            break;
        }
    }
    
    return !found_nonzero;
}

// Fast path for comparison with one
bool solid_is_one_fast(SolidNumber* s) {
    if (!s || s->barrier_type != BARRIER_EXACT) {
        return false;
    }
    
    const char* digits = solid_get_known_digits(s);
    
    // Fast check for "1"
    if (s->known_len == 1 && digits[0] == '1') {
        return true;
    }
    
    // Check for "1.0", "1.00", etc.
    if (s->known_len >= 3 && digits[0] == '1' && digits[1] == '.') {
        for (uint16_t i = 2; i < s->known_len; i++) {
            if (digits[i] != '0') {
                return false;
            }
        }
        return true;
    }
    
    return false;
}

// Fast path for exact integer comparison
int solid_compare_int_fast(SolidNumber* a, int64_t b) {
    // If not exact, use regular comparison
    if (a->barrier_type != BARRIER_EXACT) {
        SolidNumber* b_solid = solid_from_int_fast(b);
        int result = solid_compare(a, b_solid);
        solid_dec_ref(b_solid);
        return result;
    }
    
    // Fast conversion of a to integer
    const char* digits = solid_get_known_digits(a);
    int64_t val_a = 0;
    int sign = 1;
    int i = 0;
    
    if (digits[0] == '-') {
        sign = -1;
        i = 1;
    }
    
    // Check if has decimal part
    bool has_decimal = false;
    int decimal_pos = -1;
    for (int j = i; j < a->known_len; j++) {
        if (digits[j] == '.') {
            decimal_pos = j;
            has_decimal = true;
            break;
        }
    }
    
    // Parse integer part
    int end = has_decimal ? decimal_pos : a->known_len;
    for (; i < end; i++) {
        val_a = val_a * 10 + (digits[i] - '0');
    }
    val_a *= sign;
    
    // If has non-zero decimal part, comparison is affected
    if (has_decimal) {
        for (int j = decimal_pos + 1; j < a->known_len; j++) {
            if (digits[j] != '0') {
                // Has fractional part
                if (val_a < b) return -1;
                if (val_a > b) return 1;
                // val_a == b but a has fractional part
                return sign > 0 ? 1 : -1;
            }
        }
    }
    
    // Direct integer comparison
    if (val_a < b) return -1;
    if (val_a > b) return 1;
    return 0;
}

// Fast path for doubling (multiply by 2)
SolidNumber* solid_double_fast(SolidNumber* a) {
    if (a->barrier_type != BARRIER_EXACT) {
        // Non-exact - double confidence reduction
        return solid_init_with_gap(solid_get_known_digits(a), a->known_len,
                                  a->barrier_type, a->gap_magnitude,
                                  a->confidence_x1000 * 9 / 10,
                                  solid_get_terminal_digits(a), a->terminal_len,
                                  a->terminal_type);
    }
    
    // Fast doubling for exact numbers
    const char* digits = solid_get_known_digits(a);
    char result[256];
    int len = 0;
    int carry = 0;
    
    // Handle sign
    bool negative = false;
    int start = 0;
    if (digits[0] == '-') {
        negative = true;
        start = 1;
        result[len++] = '-';
    }
    
    // Double each digit from right to left
    for (int i = a->known_len - 1; i >= start; i--) {
        if (digits[i] == '.') {
            continue;  // Skip decimal point
        }
        
        int digit = (digits[i] - '0') * 2 + carry;
        carry = digit / 10;
        result[len++] = '0' + (digit % 10);
    }
    
    if (carry > 0) {
        result[len++] = '0' + carry;
    }
    
    // Reverse the result (except sign)
    int rev_start = negative ? 1 : 0;
    for (int i = 0; i < (len - rev_start) / 2; i++) {
        char temp = result[rev_start + i];
        result[rev_start + i] = result[len - 1 - i];
        result[len - 1 - i] = temp;
    }
    
    return solid_init_exact(result, len);
}

// Fast path benchmark
void solid_fastpath_benchmark(void) {
    print_str("\n=== SOLID FAST PATH BENCHMARK ===\n");
    
    uint64_t start, end;
    const int iterations = 1000000;
    
    // Initialize cache
    init_small_int_cache();
    
    // Benchmark small integer creation
    asm volatile("rdtsc" : "=A"(start));
    for (int i = 0; i < iterations; i++) {
        SolidNumber* n = solid_from_int_fast(42);
        solid_dec_ref(n);
    }
    asm volatile("rdtsc" : "=A"(end));
    
    print_str("Small integer creation (cached): ");
    print_num((end - start) / iterations);
    print_str(" cycles/op\n");
    
    // Benchmark integer addition
    SolidNumber* base = solid_from_int_fast(100);
    
    asm volatile("rdtsc" : "=A"(start));
    for (int i = 0; i < iterations; i++) {
        SolidNumber* result = solid_add_int_fast(base, 42);
        solid_dec_ref(result);
    }
    asm volatile("rdtsc" : "=A"(end));
    
    print_str("Integer addition fast path: ");
    print_num((end - start) / iterations);
    print_str(" cycles/op\n");
    
    // Benchmark multiply by power of 10
    asm volatile("rdtsc" : "=A"(start));
    for (int i = 0; i < iterations; i++) {
        SolidNumber* result = solid_multiply_pow10_fast(base, 3);
        solid_dec_ref(result);
    }
    asm volatile("rdtsc" : "=A"(end));
    
    print_str("Multiply by 10^3 fast path: ");
    print_num((end - start) / iterations);
    print_str(" cycles/op\n");
    
    // Benchmark zero check
    SolidNumber* zero = solid_from_int_fast(0);
    
    asm volatile("rdtsc" : "=A"(start));
    for (int i = 0; i < iterations; i++) {
        solid_is_zero_fast(zero);
    }
    asm volatile("rdtsc" : "=A"(end));
    
    print_str("Zero check fast path: ");
    print_num((end - start) / iterations);
    print_str(" cycles/op\n");
    
    // Cleanup
    solid_dec_ref(base);
    solid_dec_ref(zero);
}