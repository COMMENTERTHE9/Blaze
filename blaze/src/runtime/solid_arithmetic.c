// SOLID NUMBER ARITHMETIC OPERATIONS
// Implementation of addition, subtraction, multiplication, and division

#include "blaze_internals.h"
#include "solid_runtime.h"

// Helper function to add digit strings
// Returns carry (0 or 1)
static int add_digit_strings(const char* a, uint16_t a_len,
                            const char* b, uint16_t b_len,
                            char* result, uint16_t* result_len) {
    int carry = 0;
    int i = a_len - 1;
    int j = b_len - 1;
    int k = 0;
    char temp[SOLID_INLINE_DIGITS * 2];
    
    // Add from right to left
    while (i >= 0 || j >= 0 || carry) {
        int digit_a = (i >= 0 && a[i] != '.') ? (a[i] - '0') : 0;
        int digit_b = (j >= 0 && b[j] != '.') ? (b[j] - '0') : 0;
        
        // Skip decimal points
        if (i >= 0 && a[i] == '.') {
            temp[k++] = '.';
            i--;
            if (j >= 0 && b[j] == '.') j--;
            continue;
        }
        if (j >= 0 && b[j] == '.') {
            temp[k++] = '.';
            j--;
            continue;
        }
        
        int sum = digit_a + digit_b + carry;
        carry = sum / 10;
        temp[k++] = '0' + (sum % 10);
        
        if (i >= 0) i--;
        if (j >= 0) j--;
    }
    
    // Reverse the result
    *result_len = k;
    for (int idx = 0; idx < k; idx++) {
        result[idx] = temp[k - 1 - idx];
    }
    
    return carry;
}

// Helper to find minimum of two values
static inline uint64_t min_u64(uint64_t a, uint64_t b) {
    return a < b ? a : b;
}

// Helper to calculate new confidence after operation
static uint16_t combine_confidence(uint16_t conf_a, uint16_t conf_b, char op) {
    // Confidence propagation rules
    uint32_t combined;
    
    switch (op) {
        case '+':
        case '-':
            // For addition/subtraction, take minimum confidence
            combined = conf_a < conf_b ? conf_a : conf_b;
            break;
            
        case '*':
            // For multiplication, multiply confidences
            combined = ((uint32_t)conf_a * conf_b) / 1000;
            break;
            
        case '/':
            // For division, reduce confidence more
            combined = ((uint32_t)conf_a * conf_b) / 1200;
            if (combined < 100) combined = 100;  // Minimum 10%
            break;
            
        default:
            combined = conf_a;
    }
    
    return (uint16_t)combined;
}

// Helper to determine resulting barrier type
static BarrierType combine_barriers(BarrierType a, BarrierType b) {
    // If either is undefined, result is undefined
    if (a == BARRIER_UNDEFINED || b == BARRIER_UNDEFINED) {
        return BARRIER_UNDEFINED;
    }
    
    // If either is infinity, result is infinity
    if (a == BARRIER_INFINITY || b == BARRIER_INFINITY) {
        return BARRIER_INFINITY;
    }
    
    // If both are exact, result is exact
    if (a == BARRIER_EXACT && b == BARRIER_EXACT) {
        return BARRIER_EXACT;
    }
    
    // Otherwise, take the "stronger" barrier
    // Priority: quantum > energy > temporal > computational > storage
    if (a == BARRIER_QUANTUM || b == BARRIER_QUANTUM) return BARRIER_QUANTUM;
    if (a == BARRIER_ENERGY || b == BARRIER_ENERGY) return BARRIER_ENERGY;
    if (a == BARRIER_TEMPORAL || b == BARRIER_TEMPORAL) return BARRIER_TEMPORAL;
    if (a == BARRIER_COMPUTATIONAL || b == BARRIER_COMPUTATIONAL) return BARRIER_COMPUTATIONAL;
    if (a == BARRIER_STORAGE || b == BARRIER_STORAGE) return BARRIER_STORAGE;
    
    return a;  // Default
}

// SOLID NUMBER ADDITION
SolidNumber* solid_add(SolidNumber* a, SolidNumber* b) {
    if (!a || !b) return NULL;
    
    print_str("[SOLID_ADD] Adding: ");
    solid_print(a);
    print_str(" + ");
    solid_print(b);
    print_str("\n");
    
    // Special cases
    if (a->barrier_type == BARRIER_INFINITY && b->barrier_type == BARRIER_INFINITY) {
        // ∞ + ∞ = ∞
        return solid_init_with_gap("", 0, BARRIER_INFINITY, ~0ULL, 
                                  combine_confidence(a->confidence_x1000, b->confidence_x1000, '+'),
                                  NULL, 0, TERMINAL_UNDEFINED);
    }
    
    if (a->barrier_type == BARRIER_UNDEFINED || b->barrier_type == BARRIER_UNDEFINED) {
        // undefined + anything = undefined
        return solid_init_with_gap("", 0, BARRIER_UNDEFINED, 0, 100,
                                  NULL, 0, TERMINAL_UNDEFINED);
    }
    
    // For exact numbers, perform regular addition
    if (a->barrier_type == BARRIER_EXACT && b->barrier_type == BARRIER_EXACT) {
        char result_digits[SOLID_INLINE_DIGITS * 2];
        uint16_t result_len;
        
        int carry = add_digit_strings(
            solid_get_known_digits(a), a->known_len,
            solid_get_known_digits(b), b->known_len,
            result_digits, &result_len
        );
        
        // Handle carry by prepending
        if (carry) {
            for (int i = result_len; i > 0; i--) {
                result_digits[i] = result_digits[i-1];
            }
            result_digits[0] = '1';
            result_len++;
        }
        
        return solid_init_exact(result_digits, result_len);
    }
    
    // For numbers with gaps, add known parts and handle terminals
    char known_result[SOLID_INLINE_DIGITS * 2];
    uint16_t known_len;
    
    // Add known parts
    int carry = add_digit_strings(
        solid_get_known_digits(a), a->known_len,
        solid_get_known_digits(b), b->known_len,
        known_result, &known_len
    );
    
    // Handle carry
    if (carry) {
        for (int i = known_len; i > 0; i--) {
            known_result[i] = known_result[i-1];
        }
        known_result[0] = '1';
        known_len++;
    }
    
    // Determine result barrier and gap
    BarrierType result_barrier = combine_barriers(a->barrier_type, b->barrier_type);
    uint64_t result_gap = a->gap_magnitude > b->gap_magnitude ? a->gap_magnitude : b->gap_magnitude;
    uint16_t result_confidence = combine_confidence(a->confidence_x1000, b->confidence_x1000, '+');
    
    // Terminal handling
    TerminalType result_term_type = TERMINAL_DIGITS;
    char terminal_result[SOLID_MAX_TERMINAL_DIGITS];
    uint16_t terminal_len = 0;
    
    // If either has superposition or undefined terminals, result does too
    if (a->terminal_type == TERMINAL_SUPERPOSITION || b->terminal_type == TERMINAL_SUPERPOSITION) {
        result_term_type = TERMINAL_SUPERPOSITION;
    } else if (a->terminal_type == TERMINAL_UNDEFINED || b->terminal_type == TERMINAL_UNDEFINED) {
        result_term_type = TERMINAL_UNDEFINED;
    } else if (a->terminal_type == TERMINAL_DIGITS && b->terminal_type == TERMINAL_DIGITS) {
        // Add terminal digits with modular arithmetic
        // For simplicity, just concatenate for now
        // TODO: Implement proper modular addition
        const char* term_a = solid_get_terminal_digits(a);
        const char* term_b = solid_get_terminal_digits(b);
        
        if (a->terminal_len > 0 && b->terminal_len > 0) {
            // Take first few digits from each
            uint16_t half = SOLID_MAX_TERMINAL_DIGITS / 2;
            uint16_t from_a = a->terminal_len < half ? a->terminal_len : half;
            uint16_t from_b = b->terminal_len < half ? b->terminal_len : half;
            
            for (uint16_t i = 0; i < from_a; i++) {
                terminal_result[terminal_len++] = term_a[i];
            }
            for (uint16_t i = 0; i < from_b && terminal_len < SOLID_MAX_TERMINAL_DIGITS; i++) {
                terminal_result[terminal_len++] = term_b[i];
            }
        }
    }
    
    return solid_init_with_gap(known_result, known_len,
                              result_barrier, result_gap,
                              result_confidence,
                              terminal_result, terminal_len,
                              result_term_type);
}

// SOLID NUMBER SUBTRACTION
SolidNumber* solid_subtract(SolidNumber* a, SolidNumber* b) {
    if (!a || !b) return NULL;
    
    print_str("[SOLID_SUB] Subtracting: ");
    solid_print(a);
    print_str(" - ");
    solid_print(b);
    print_str("\n");
    
    // Special cases
    if (a->barrier_type == BARRIER_INFINITY && b->barrier_type == BARRIER_INFINITY) {
        // ∞ - ∞ = all natural numbers (represented as undefined)
        return solid_init_with_gap("ℕ", 1, BARRIER_UNDEFINED, ~0ULL,
                                  combine_confidence(a->confidence_x1000, b->confidence_x1000, '-'),
                                  NULL, 0, TERMINAL_SUPERPOSITION);
    }
    
    if (a->barrier_type == BARRIER_UNDEFINED || b->barrier_type == BARRIER_UNDEFINED) {
        return solid_init_with_gap("", 0, BARRIER_UNDEFINED, 0, 100,
                                  NULL, 0, TERMINAL_UNDEFINED);
    }
    
    // For exact numbers, check if b > a
    if (a->barrier_type == BARRIER_EXACT && b->barrier_type == BARRIER_EXACT) {
        // Simple comparison (not fully implemented)
        double val_a = solid_to_double(a);
        double val_b = solid_to_double(b);
        
        if (val_b > val_a) {
            // Result is negative - for now return undefined
            return solid_init_with_gap("-", 1, BARRIER_UNDEFINED, 0, 500,
                                      NULL, 0, TERMINAL_UNDEFINED);
        }
        
        // Perform subtraction (simplified)
        double result = val_a - val_b;
        char buffer[32];
        int len = 0;
        
        // Convert to string (simplified)
        int int_part = (int)result;
        double frac_part = result - int_part;
        
        // Integer part
        if (int_part == 0) {
            buffer[len++] = '0';
        } else {
            char temp[16];
            int temp_len = 0;
            while (int_part > 0) {
                temp[temp_len++] = '0' + (int_part % 10);
                int_part /= 10;
            }
            for (int i = temp_len - 1; i >= 0; i--) {
                buffer[len++] = temp[i];
            }
        }
        
        // Add decimal if needed
        if (frac_part > 0.0001) {
            buffer[len++] = '.';
            for (int i = 0; i < 4 && frac_part > 0.0001; i++) {
                frac_part *= 10;
                int digit = (int)frac_part;
                buffer[len++] = '0' + digit;
                frac_part -= digit;
            }
        }
        
        return solid_init_exact(buffer, len);
    }
    
    // For numbers with gaps
    BarrierType result_barrier = combine_barriers(a->barrier_type, b->barrier_type);
    uint64_t result_gap = a->gap_magnitude > b->gap_magnitude ? a->gap_magnitude : b->gap_magnitude;
    uint16_t result_confidence = combine_confidence(a->confidence_x1000, b->confidence_x1000, '-');
    
    // Simplified: just use a's known digits with reduced confidence
    return solid_init_with_gap(solid_get_known_digits(a), a->known_len,
                              result_barrier, result_gap,
                              result_confidence,
                              NULL, 0, TERMINAL_SUPERPOSITION);
}

// SOLID NUMBER MULTIPLICATION
SolidNumber* solid_multiply(SolidNumber* a, SolidNumber* b) {
    if (!a || !b) return NULL;
    
    print_str("[SOLID_MUL] Multiplying: ");
    solid_print(a);
    print_str(" * ");
    solid_print(b);
    print_str("\n");
    
    // Special cases
    if (a->barrier_type == BARRIER_INFINITY || b->barrier_type == BARRIER_INFINITY) {
        // ∞ * anything (except 0) = ∞
        // For simplicity, assume non-zero
        return solid_init_with_gap("", 0, BARRIER_INFINITY, ~0ULL,
                                  combine_confidence(a->confidence_x1000, b->confidence_x1000, '*'),
                                  NULL, 0, TERMINAL_UNDEFINED);
    }
    
    if (a->barrier_type == BARRIER_UNDEFINED || b->barrier_type == BARRIER_UNDEFINED) {
        return solid_init_with_gap("", 0, BARRIER_UNDEFINED, 0, 100,
                                  NULL, 0, TERMINAL_UNDEFINED);
    }
    
    // For exact numbers
    if (a->barrier_type == BARRIER_EXACT && b->barrier_type == BARRIER_EXACT) {
        // Simplified multiplication using doubles
        double val_a = solid_to_double(a);
        double val_b = solid_to_double(b);
        double result = val_a * val_b;
        
        // Convert back to string
        char buffer[64];
        int len = 0;
        
        if (result < 0) {
            buffer[len++] = '-';
            result = -result;
        }
        
        // Integer part
        int int_part = (int)result;
        double frac_part = result - int_part;
        
        if (int_part == 0) {
            buffer[len++] = '0';
        } else {
            char temp[32];
            int temp_len = 0;
            while (int_part > 0) {
                temp[temp_len++] = '0' + (int_part % 10);
                int_part /= 10;
            }
            for (int i = temp_len - 1; i >= 0; i--) {
                buffer[len++] = temp[i];
            }
        }
        
        // Decimal part
        if (frac_part > 0.0001) {
            buffer[len++] = '.';
            for (int i = 0; i < 6 && frac_part > 0.0001; i++) {
                frac_part *= 10;
                int digit = (int)frac_part;
                buffer[len++] = '0' + digit;
                frac_part -= digit;
            }
        }
        
        return solid_init_exact(buffer, len);
    }
    
    // For numbers with gaps
    // Gap magnitude increases (multiplicative)
    uint64_t new_gap = a->gap_magnitude;
    if (b->gap_magnitude > 1 && new_gap < ~0ULL / b->gap_magnitude) {
        new_gap *= b->gap_magnitude;
    } else {
        new_gap = ~0ULL;  // Overflow to infinity
    }
    
    BarrierType result_barrier = combine_barriers(a->barrier_type, b->barrier_type);
    uint16_t result_confidence = combine_confidence(a->confidence_x1000, b->confidence_x1000, '*');
    
    // For simplicity, multiply known parts only
    double known_a = solid_to_double(a);
    double known_b = solid_to_double(b);
    double known_result = known_a * known_b;
    
    char buffer[32];
    int len = 0;
    int int_part = (int)known_result;
    
    if (int_part == 0) {
        buffer[len++] = '0';
    } else {
        char temp[16];
        int temp_len = 0;
        while (int_part > 0) {
            temp[temp_len++] = '0' + (int_part % 10);
            int_part /= 10;
        }
        for (int i = temp_len - 1; i >= 0; i--) {
            buffer[len++] = temp[i];
        }
    }
    
    return solid_init_with_gap(buffer, len,
                              result_barrier, new_gap,
                              result_confidence,
                              NULL, 0, TERMINAL_SUPERPOSITION);
}

// SOLID NUMBER DIVISION
SolidNumber* solid_divide(SolidNumber* a, SolidNumber* b) {
    if (!a || !b) return NULL;
    
    print_str("[SOLID_DIV] Dividing: ");
    solid_print(a);
    print_str(" / ");
    solid_print(b);
    print_str("\n");
    
    // Check for division by zero
    double val_b = solid_to_double(b);
    if (val_b == 0.0) {
        // Division by zero
        if (solid_is_exact(b)) {
            // Exact zero - undefined
            return solid_init_with_gap("", 0, BARRIER_UNDEFINED, 0, 0,
                                      NULL, 0, TERMINAL_UNDEFINED);
        } else {
            // Might not be zero - very low confidence
            return solid_init_with_gap("", 0, BARRIER_COMPUTATIONAL, ~0ULL, 50,
                                      NULL, 0, TERMINAL_SUPERPOSITION);
        }
    }
    
    // Special cases
    if (a->barrier_type == BARRIER_INFINITY && b->barrier_type == BARRIER_INFINITY) {
        // ∞ / ∞ has special algorithm in solid numbers
        // Result depends on terminal digits comparison
        return solid_init_with_gap("1", 1, BARRIER_COMPUTATIONAL, 1000000, 750,
                                  NULL, 0, TERMINAL_SUPERPOSITION);
    }
    
    if (a->barrier_type == BARRIER_INFINITY) {
        return solid_init_with_gap("", 0, BARRIER_INFINITY, ~0ULL,
                                  combine_confidence(a->confidence_x1000, b->confidence_x1000, '/'),
                                  NULL, 0, TERMINAL_UNDEFINED);
    }
    
    if (b->barrier_type == BARRIER_INFINITY) {
        // finite / ∞ = 0...
        return solid_init_with_gap("0", 1, BARRIER_COMPUTATIONAL, 1,
                                  combine_confidence(a->confidence_x1000, b->confidence_x1000, '/'),
                                  NULL, 0, TERMINAL_DIGITS);
    }
    
    // For exact numbers
    if (a->barrier_type == BARRIER_EXACT && b->barrier_type == BARRIER_EXACT) {
        double val_a = solid_to_double(a);
        double result = val_a / val_b;
        
        // Check if result is exact (integer division)
        if (result == (double)(int)result) {
            char buffer[32];
            int len = 0;
            int int_result = (int)result;
            
            if (int_result == 0) {
                buffer[len++] = '0';
            } else {
                char temp[16];
                int temp_len = 0;
                while (int_result > 0) {
                    temp[temp_len++] = '0' + (int_result % 10);
                    int_result /= 10;
                }
                for (int i = temp_len - 1; i >= 0; i--) {
                    buffer[len++] = temp[i];
                }
            }
            
            return solid_init_exact(buffer, len);
        } else {
            // Result has infinite decimal expansion - use computational barrier
            char buffer[32];
            int len = 0;
            
            // Format first few digits
            int int_part = (int)result;
            double frac_part = result - int_part;
            
            if (int_part == 0) {
                buffer[len++] = '0';
            } else {
                char temp[16];
                int temp_len = 0;
                while (int_part > 0) {
                    temp[temp_len++] = '0' + (int_part % 10);
                    int_part /= 10;
                }
                for (int i = temp_len - 1; i >= 0; i--) {
                    buffer[len++] = temp[i];
                }
            }
            
            buffer[len++] = '.';
            for (int i = 0; i < 6 && len < 30; i++) {
                frac_part *= 10;
                int digit = (int)frac_part;
                buffer[len++] = '0' + digit;
                frac_part -= digit;
            }
            
            // Division often creates repeating decimals
            return solid_init_with_gap(buffer, len,
                                      BARRIER_COMPUTATIONAL, 1000000, 900,
                                      NULL, 0, TERMINAL_DIGITS);
        }
    }
    
    // For numbers with gaps - use modular inverse for terminals
    BarrierType result_barrier = combine_barriers(a->barrier_type, b->barrier_type);
    if (result_barrier == BARRIER_EXACT) {
        result_barrier = BARRIER_COMPUTATIONAL;  // Division rarely preserves exactness
    }
    
    uint64_t result_gap = a->gap_magnitude;
    // Division can increase uncertainty significantly
    if (result_gap < ~0ULL / 10) {
        result_gap *= 10;
    } else {
        result_gap = ~0ULL;
    }
    
    uint16_t result_confidence = combine_confidence(a->confidence_x1000, b->confidence_x1000, '/');
    
    // Simplified division of known parts
    double known_a = solid_to_double(a);
    double known_b = solid_to_double(b);
    double known_result = known_a / known_b;
    
    char buffer[32];
    int len = 0;
    int int_part = (int)known_result;
    
    if (int_part == 0) {
        buffer[len++] = '0';
    } else {
        char temp[16];
        int temp_len = 0;
        while (int_part > 0) {
            temp[temp_len++] = '0' + (int_part % 10);
            int_part /= 10;
        }
        for (int i = temp_len - 1; i >= 0; i--) {
            buffer[len++] = temp[i];
        }
    }
    
    // For terminal digits, would implement modular inverse
    // For now, mark as superposition
    return solid_init_with_gap(buffer, len,
                              result_barrier, result_gap,
                              result_confidence,
                              NULL, 0, TERMINAL_SUPERPOSITION);
}