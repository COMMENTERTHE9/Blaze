// SOLID NUMBER UNDEFINED HANDLING
// Operations and propagation rules for undefined values

#include "blaze_internals.h"
#include "solid_runtime.h"

// Undefined reason codes
typedef enum {
    UNDEFINED_DIVISION_BY_ZERO,
    UNDEFINED_ZERO_TO_ZERO,
    UNDEFINED_INFINITY_MINUS_INFINITY,  // Wait, this gives ℕ, not undefined
    UNDEFINED_ZERO_TIMES_INFINITY,
    UNDEFINED_SQRT_NEGATIVE,
    UNDEFINED_LOG_NONPOSITIVE,
    UNDEFINED_INDETERMINATE_FORM,
    UNDEFINED_COMPUTATIONAL_OVERFLOW,
    UNDEFINED_BARRIER_VIOLATION,
    UNDEFINED_TERMINAL_CONFLICT,
    UNDEFINED_CONFIDENCE_ZERO,
    UNDEFINED_UNKNOWN
} UndefinedReason;

// Undefined metadata
typedef struct {
    UndefinedReason reason;
    char description[256];
    SolidNumber* operand_a;
    SolidNumber* operand_b;
    char operation;
} UndefinedMetadata;

// Global undefined metadata storage
static UndefinedMetadata undefined_metadata[100];
static int next_metadata_idx = 0;

// Get reason string
static const char* undefined_reason_string(UndefinedReason reason) {
    switch (reason) {
        case UNDEFINED_DIVISION_BY_ZERO:
            return "division by zero";
        case UNDEFINED_ZERO_TO_ZERO:
            return "0^0 indeterminate form";
        case UNDEFINED_ZERO_TIMES_INFINITY:
            return "0 × ∞ indeterminate form";
        case UNDEFINED_SQRT_NEGATIVE:
            return "square root of negative number";
        case UNDEFINED_LOG_NONPOSITIVE:
            return "logarithm of non-positive number";
        case UNDEFINED_INDETERMINATE_FORM:
            return "indeterminate mathematical form";
        case UNDEFINED_COMPUTATIONAL_OVERFLOW:
            return "computational overflow";
        case UNDEFINED_BARRIER_VIOLATION:
            return "barrier type violation";
        case UNDEFINED_TERMINAL_CONFLICT:
            return "conflicting terminal digits";
        case UNDEFINED_CONFIDENCE_ZERO:
            return "confidence reduced to zero";
        case UNDEFINED_UNKNOWN:
        default:
            return "unknown undefined condition";
    }
}

// Create undefined solid number with metadata
SolidNumber* solid_undefined_with_reason(UndefinedReason reason, 
                                        const char* details,
                                        SolidNumber* operand_a,
                                        SolidNumber* operand_b,
                                        char operation) {
    // Store metadata
    UndefinedMetadata* meta = &undefined_metadata[next_metadata_idx];
    next_metadata_idx = (next_metadata_idx + 1) % 100;
    
    meta->reason = reason;
    meta->operand_a = operand_a;
    meta->operand_b = operand_b;
    meta->operation = operation;
    
    // Copy description
    int i = 0;
    if (details) {
        while (details[i] && i < 255) {
            meta->description[i] = details[i];
            i++;
        }
    }
    meta->description[i] = '\0';
    
    print_str("[SOLID-UNDEFINED] Creating undefined: ");
    print_str(undefined_reason_string(reason));
    if (details) {
        print_str(" - ");
        print_str(details);
    }
    print_str("\n");
    
    // Create undefined solid number
    return solid_init_with_gap("", 0, BARRIER_UNDEFINED, 0, 0,
                              NULL, 0, TERMINAL_UNDEFINED);
}

// Check if operation would produce undefined
bool solid_would_be_undefined(SolidNumber* a, SolidNumber* b, char op) {
    if (!a || !b) return true;
    
    // Any operation with undefined propagates undefined
    if (a->barrier_type == BARRIER_UNDEFINED || 
        b->barrier_type == BARRIER_UNDEFINED) {
        return true;
    }
    
    // Check specific operations
    switch (op) {
        case '/':
            // Division by zero
            if (solid_is_zero(b)) {
                return true;
            }
            // Note: ∞ ÷ ∞ is NOT undefined - it has a specific algorithm
            break;
            
        case '^':
            // 0^0 is undefined
            if (solid_is_zero(a) && solid_is_zero(b)) {
                return true;
            }
            // Negative base with non-integer exponent
            if (solid_is_negative(a) && !solid_is_integer(b)) {
                return true;
            }
            break;
            
        case '*':
            // 0 × ∞ is undefined
            if ((solid_is_zero(a) && solid_is_infinity(b)) ||
                (solid_is_infinity(a) && solid_is_zero(b))) {
                return true;
            }
            break;
            
        case '-':
            // Note: ∞ - ∞ is NOT undefined - it equals ℕ
            break;
    }
    
    // Check for confidence collapse
    if (a->confidence_x1000 == 0 || b->confidence_x1000 == 0) {
        return true;
    }
    
    return false;
}

// Undefined arithmetic operations
SolidNumber* solid_undefined_add(SolidNumber* a, SolidNumber* b) {
    // If either is undefined, result is undefined
    if (a->barrier_type == BARRIER_UNDEFINED) {
        return solid_undefined_with_reason(UNDEFINED_UNKNOWN, 
                                         "propagated from first operand",
                                         a, b, '+');
    }
    if (b->barrier_type == BARRIER_UNDEFINED) {
        return solid_undefined_with_reason(UNDEFINED_UNKNOWN,
                                         "propagated from second operand", 
                                         a, b, '+');
    }
    
    // Addition rarely produces undefined
    return NULL;
}

SolidNumber* solid_undefined_multiply(SolidNumber* a, SolidNumber* b) {
    // Check for 0 × ∞
    if ((solid_is_zero(a) && solid_is_infinity(b)) ||
        (solid_is_infinity(a) && solid_is_zero(b))) {
        return solid_undefined_with_reason(UNDEFINED_ZERO_TIMES_INFINITY,
                                         "0 × ∞ indeterminate form",
                                         a, b, '*');
    }
    
    // Propagate undefined
    if (a->barrier_type == BARRIER_UNDEFINED || 
        b->barrier_type == BARRIER_UNDEFINED) {
        return solid_undefined_with_reason(UNDEFINED_UNKNOWN,
                                         "undefined propagation",
                                         a, b, '*');
    }
    
    return NULL;
}

SolidNumber* solid_undefined_divide(SolidNumber* a, SolidNumber* b) {
    // Division by zero
    if (solid_is_zero(b)) {
        if (solid_is_exact(b)) {
            return solid_undefined_with_reason(UNDEFINED_DIVISION_BY_ZERO,
                                             "exact division by zero",
                                             a, b, '/');
        } else {
            // Might not actually be zero due to gap
            return solid_undefined_with_reason(UNDEFINED_DIVISION_BY_ZERO,
                                             "probable division by zero",
                                             a, b, '/');
        }
    }
    
    // Propagate undefined
    if (a->barrier_type == BARRIER_UNDEFINED || 
        b->barrier_type == BARRIER_UNDEFINED) {
        return solid_undefined_with_reason(UNDEFINED_UNKNOWN,
                                         "undefined propagation",
                                         a, b, '/');
    }
    
    return NULL;
}

SolidNumber* solid_undefined_power(SolidNumber* base, SolidNumber* exp) {
    // 0^0 is undefined
    if (solid_is_zero(base) && solid_is_zero(exp)) {
        return solid_undefined_with_reason(UNDEFINED_ZERO_TO_ZERO,
                                         "0^0 indeterminate form",
                                         base, exp, '^');
    }
    
    // Negative base with non-integer exponent
    if (solid_is_negative(base) && !solid_is_integer(exp)) {
        return solid_undefined_with_reason(UNDEFINED_SQRT_NEGATIVE,
                                         "negative base with fractional exponent",
                                         base, exp, '^');
    }
    
    // Propagate undefined
    if (base->barrier_type == BARRIER_UNDEFINED || 
        exp->barrier_type == BARRIER_UNDEFINED) {
        return solid_undefined_with_reason(UNDEFINED_UNKNOWN,
                                         "undefined propagation",
                                         base, exp, '^');
    }
    
    return NULL;
}

// Mathematical functions that can produce undefined
SolidNumber* solid_sqrt(SolidNumber* x) {
    if (!x) return NULL;
    
    // Propagate undefined
    if (x->barrier_type == BARRIER_UNDEFINED) {
        return solid_undefined_with_reason(UNDEFINED_UNKNOWN,
                                         "undefined propagation",
                                         x, NULL, 'r');  // 'r' for root
    }
    
    // Square root of negative
    if (solid_is_negative(x)) {
        return solid_undefined_with_reason(UNDEFINED_SQRT_NEGATIVE,
                                         "square root of negative number",
                                         x, NULL, 'r');
    }
    
    // For now, return placeholder
    // TODO: Implement actual square root
    double val = solid_to_double(x);
    if (val < 0) {
        return solid_undefined_with_reason(UNDEFINED_SQRT_NEGATIVE,
                                         "square root of negative",
                                         x, NULL, 'r');
    }
    
    // Simple approximation
    double result = 1.0;
    for (int i = 0; i < 10; i++) {
        result = (result + val / result) / 2.0;
    }
    
    char buffer[32];
    int len = 0;
    int int_part = (int)result;
    double frac_part = result - int_part;
    
    // Format result
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
    
    if (frac_part > 0.0001) {
        buffer[len++] = '.';
        for (int i = 0; i < 6; i++) {
            frac_part *= 10;
            int digit = (int)frac_part;
            buffer[len++] = '0' + digit;
            frac_part -= digit;
        }
    }
    
    // Square roots often have computational barriers
    return solid_init_with_gap(buffer, len, BARRIER_COMPUTATIONAL, 1000000,
                              x->confidence_x1000 * 9 / 10,
                              NULL, 0, TERMINAL_DIGITS);
}

SolidNumber* solid_log(SolidNumber* x) {
    if (!x) return NULL;
    
    // Propagate undefined
    if (x->barrier_type == BARRIER_UNDEFINED) {
        return solid_undefined_with_reason(UNDEFINED_UNKNOWN,
                                         "undefined propagation",
                                         x, NULL, 'l');  // 'l' for log
    }
    
    // Log of non-positive
    if (solid_is_zero(x) || solid_is_negative(x)) {
        return solid_undefined_with_reason(UNDEFINED_LOG_NONPOSITIVE,
                                         "logarithm of non-positive number",
                                         x, NULL, 'l');
    }
    
    // TODO: Implement actual logarithm
    // For now, return undefined for simplicity
    return solid_undefined_with_reason(UNDEFINED_UNKNOWN,
                                     "logarithm not yet implemented",
                                     x, NULL, 'l');
}

// Check various properties
bool solid_is_zero(SolidNumber* s) {
    if (!s) return false;
    
    // Must be exact to be sure it's zero
    if (s->barrier_type != BARRIER_EXACT) {
        return false;
    }
    
    const char* digits = solid_get_known_digits(s);
    
    // Skip minus sign if present
    int start = 0;
    if (digits[0] == '-') start = 1;
    
    // Check all digits are zero or decimal point
    for (uint16_t i = start; i < s->known_len; i++) {
        if (digits[i] != '0' && digits[i] != '.') {
            return false;
        }
    }
    
    return true;
}

bool solid_is_negative(SolidNumber* s) {
    if (!s) return false;
    
    const char* digits = solid_get_known_digits(s);
    return digits[0] == '-';
}

bool solid_is_integer(SolidNumber* s) {
    if (!s) return false;
    
    // Must be exact to be sure it's integer
    if (s->barrier_type != BARRIER_EXACT) {
        return false;
    }
    
    const char* digits = solid_get_known_digits(s);
    
    // Check for decimal point
    for (uint16_t i = 0; i < s->known_len; i++) {
        if (digits[i] == '.') {
            // Check if all digits after decimal are zero
            for (uint16_t j = i + 1; j < s->known_len; j++) {
                if (digits[j] != '0') {
                    return false;
                }
            }
            return true;
        }
    }
    
    // No decimal point - it's an integer
    return true;
}

// Get undefined metadata
const char* solid_undefined_reason(SolidNumber* s) {
    if (!s || s->barrier_type != BARRIER_UNDEFINED) {
        return NULL;
    }
    
    // Search for metadata
    for (int i = 0; i < 100; i++) {
        // This is a heuristic - in production we'd track this better
        if (undefined_metadata[i].operand_a == s ||
            undefined_metadata[i].operand_b == s) {
            return undefined_metadata[i].description;
        }
    }
    
    return "unknown undefined reason";
}

// Undefined recovery strategies
SolidNumber* solid_recover_from_undefined(SolidNumber* undef, 
                                        RecoveryStrategy strategy) {
    if (!undef || undef->barrier_type != BARRIER_UNDEFINED) {
        return undef;  // Not undefined, return as-is
    }
    
    print_str("[SOLID-UNDEFINED] Attempting recovery with strategy: ");
    
    switch (strategy) {
        case RECOVERY_USE_ZERO:
            print_str("use zero\n");
            return solid_init_exact("0", 1);
            
        case RECOVERY_USE_ONE:
            print_str("use one\n");
            return solid_init_exact("1", 1);
            
        case RECOVERY_USE_INFINITY:
            print_str("use infinity\n");
            return solid_init_with_gap("", 0, BARRIER_INFINITY, ~0ULL, 500,
                                     NULL, 0, TERMINAL_UNDEFINED);
            
        case RECOVERY_USE_NAN:
            print_str("use NaN\n");
            return solid_init_with_gap("NaN", 3, BARRIER_UNDEFINED, 0, 0,
                                     NULL, 0, TERMINAL_UNDEFINED);
            
        case RECOVERY_PROPAGATE:
        default:
            print_str("propagate\n");
            solid_inc_ref(undef);
            return undef;
    }
}