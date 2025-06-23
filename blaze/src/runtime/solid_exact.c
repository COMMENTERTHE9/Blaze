// SOLID NUMBER EXACT ARITHMETIC
// Special handling for exact numbers with no computational barriers

#include "blaze_internals.h"
#include "solid_runtime.h"

// Arbitrary precision integer for exact arithmetic
typedef struct {
    uint32_t* digits;    // Array of digit groups (base 10^9)
    uint32_t length;     // Number of digit groups
    uint32_t capacity;   // Allocated capacity
    bool negative;       // Sign
} BigInt;

// Forward declarations
static uint32_t power_of_10(int n);

// Helper to allocate BigInt
static BigInt* bigint_alloc(uint32_t capacity) {
    static BigInt storage[100];  // Static pool
    static int next_idx = 0;
    
    if (next_idx >= 100) next_idx = 0;  // Wrap around
    
    BigInt* bi = &storage[next_idx++];
    
    // Use static digit storage
    static uint32_t digit_storage[10000];
    static int digit_idx = 0;
    
    if (digit_idx + capacity > 10000) digit_idx = 0;  // Wrap
    
    bi->digits = &digit_storage[digit_idx];
    digit_idx += capacity;
    
    bi->length = 0;
    bi->capacity = capacity;
    bi->negative = false;
    
    // Clear digits
    for (uint32_t i = 0; i < capacity; i++) {
        bi->digits[i] = 0;
    }
    
    return bi;
}

// Convert string to BigInt
static BigInt* string_to_bigint(const char* str, uint32_t len) {
    BigInt* result = bigint_alloc(20);
    
    // Handle sign
    uint32_t start = 0;
    if (str[0] == '-') {
        result->negative = true;
        start = 1;
    }
    
    // Process digits in groups of 9
    uint32_t group = 0;
    uint32_t multiplier = 1;
    
    for (int i = len - 1; i >= (int)start; i--) {
        if (str[i] >= '0' && str[i] <= '9') {
            group += (str[i] - '0') * multiplier;
            multiplier *= 10;
            
            if (multiplier == 1000000000 || i == (int)start) {
                // Store group
                if (result->length < result->capacity) {
                    result->digits[result->length++] = group;
                }
                group = 0;
                multiplier = 1;
            }
        }
    }
    
    // Remove leading zeros
    while (result->length > 1 && result->digits[result->length - 1] == 0) {
        result->length--;
    }
    
    return result;
}

// Convert BigInt to string
static void bigint_to_string(BigInt* bi, char* buffer, uint32_t* len) {
    *len = 0;
    
    if (bi->negative) {
        buffer[(*len)++] = '-';
    }
    
    // Convert each group
    bool first = true;
    for (int i = bi->length - 1; i >= 0; i--) {
        uint32_t group = bi->digits[i];
        
        if (first) {
            // First group - no padding
            char temp[10];
            int temp_len = 0;
            
            if (group == 0) {
                temp[temp_len++] = '0';
            } else {
                while (group > 0) {
                    temp[temp_len++] = '0' + (group % 10);
                    group /= 10;
                }
            }
            
            // Reverse
            for (int j = temp_len - 1; j >= 0; j--) {
                buffer[(*len)++] = temp[j];
            }
            first = false;
        } else {
            // Subsequent groups - pad to 9 digits
            for (int j = 8; j >= 0; j--) {
                buffer[(*len)++] = '0' + ((group / power_of_10(j)) % 10);
            }
        }
    }
    
    if (*len == 0 || (bi->negative && *len == 1)) {
        buffer[(*len)++] = '0';
    }
}

// Helper for power of 10
static uint32_t power_of_10(int n) {
    uint32_t result = 1;
    for (int i = 0; i < n; i++) {
        result *= 10;
    }
    return result;
}

// Add two BigInts (ignoring sign)
static BigInt* bigint_add_unsigned(BigInt* a, BigInt* b) {
    BigInt* result = bigint_alloc(a->length > b->length ? a->length + 1 : b->length + 1);
    
    uint64_t carry = 0;
    uint32_t i = 0;
    
    // Add common digits
    while (i < a->length && i < b->length) {
        uint64_t sum = (uint64_t)a->digits[i] + b->digits[i] + carry;
        result->digits[i] = sum % 1000000000;
        carry = sum / 1000000000;
        i++;
    }
    
    // Add remaining digits from a
    while (i < a->length) {
        uint64_t sum = (uint64_t)a->digits[i] + carry;
        result->digits[i] = sum % 1000000000;
        carry = sum / 1000000000;
        i++;
    }
    
    // Add remaining digits from b
    while (i < b->length) {
        uint64_t sum = (uint64_t)b->digits[i] + carry;
        result->digits[i] = sum % 1000000000;
        carry = sum / 1000000000;
        i++;
    }
    
    // Add final carry
    if (carry > 0 && i < result->capacity) {
        result->digits[i++] = carry;
    }
    
    result->length = i;
    return result;
}

// Compare two BigInts (ignoring sign)
static int bigint_compare_unsigned(BigInt* a, BigInt* b) {
    if (a->length != b->length) {
        return a->length > b->length ? 1 : -1;
    }
    
    for (int i = a->length - 1; i >= 0; i--) {
        if (a->digits[i] != b->digits[i]) {
            return a->digits[i] > b->digits[i] ? 1 : -1;
        }
    }
    
    return 0;
}

// Subtract two BigInts (a - b, assuming a >= b)
static BigInt* bigint_subtract_unsigned(BigInt* a, BigInt* b) {
    BigInt* result = bigint_alloc(a->length);
    
    int64_t borrow = 0;
    uint32_t i = 0;
    
    // Subtract common digits
    while (i < b->length) {
        int64_t diff = (int64_t)a->digits[i] - b->digits[i] - borrow;
        if (diff < 0) {
            diff += 1000000000;
            borrow = 1;
        } else {
            borrow = 0;
        }
        result->digits[i] = diff;
        i++;
    }
    
    // Subtract borrow from remaining digits
    while (i < a->length) {
        int64_t diff = (int64_t)a->digits[i] - borrow;
        if (diff < 0) {
            diff += 1000000000;
            borrow = 1;
        } else {
            borrow = 0;
        }
        result->digits[i] = diff;
        i++;
    }
    
    result->length = a->length;
    
    // Remove leading zeros
    while (result->length > 1 && result->digits[result->length - 1] == 0) {
        result->length--;
    }
    
    return result;
}

// Multiply two BigInts
static BigInt* bigint_multiply(BigInt* a, BigInt* b) {
    BigInt* result = bigint_alloc(a->length + b->length);
    
    // School multiplication
    for (uint32_t i = 0; i < a->length; i++) {
        uint64_t carry = 0;
        for (uint32_t j = 0; j < b->length; j++) {
            if (i + j < result->capacity) {
                uint64_t prod = (uint64_t)a->digits[i] * b->digits[j] + 
                               result->digits[i + j] + carry;
                result->digits[i + j] = prod % 1000000000;
                carry = prod / 1000000000;
            }
        }
        
        // Add final carry
        uint32_t k = i + b->length;
        while (carry > 0 && k < result->capacity) {
            uint64_t sum = result->digits[k] + carry;
            result->digits[k] = sum % 1000000000;
            carry = sum / 1000000000;
            k++;
        }
    }
    
    // Find actual length
    result->length = a->length + b->length;
    while (result->length > 1 && result->digits[result->length - 1] == 0) {
        result->length--;
    }
    
    result->negative = a->negative != b->negative;
    return result;
}

// Exact addition for solid numbers
SolidNumber* solid_exact_add(SolidNumber* a, SolidNumber* b) {
    print_str("[SOLID-EXACT] Adding exact numbers\n");
    
    // Extract digits
    const char* a_digits = solid_get_known_digits(a);
    const char* b_digits = solid_get_known_digits(b);
    
    // Convert to BigInt
    BigInt* bi_a = string_to_bigint(a_digits, a->known_len);
    BigInt* bi_b = string_to_bigint(b_digits, b->known_len);
    
    // Perform addition
    BigInt* result;
    if (bi_a->negative == bi_b->negative) {
        // Same sign - add magnitudes
        result = bigint_add_unsigned(bi_a, bi_b);
        result->negative = bi_a->negative;
    } else {
        // Different signs - subtract magnitudes
        int cmp = bigint_compare_unsigned(bi_a, bi_b);
        if (cmp >= 0) {
            result = bigint_subtract_unsigned(bi_a, bi_b);
            result->negative = bi_a->negative;
        } else {
            result = bigint_subtract_unsigned(bi_b, bi_a);
            result->negative = bi_b->negative;
        }
    }
    
    // Convert back to string
    char buffer[256];
    uint32_t len;
    bigint_to_string(result, buffer, &len);
    
    return solid_init_exact(buffer, len);
}

// Exact subtraction
SolidNumber* solid_exact_subtract(SolidNumber* a, SolidNumber* b) {
    print_str("[SOLID-EXACT] Subtracting exact numbers\n");
    
    // Flip sign of b and add
    const char* b_digits = solid_get_known_digits(b);
    char neg_b[256];
    uint32_t len = 0;
    
    if (b_digits[0] == '-') {
        // b is negative, make positive
        for (uint32_t i = 1; i < b->known_len; i++) {
            neg_b[len++] = b_digits[i];
        }
    } else {
        // b is positive, make negative
        neg_b[len++] = '-';
        for (uint32_t i = 0; i < b->known_len; i++) {
            neg_b[len++] = b_digits[i];
        }
    }
    
    SolidNumber* neg_b_solid = solid_init_exact(neg_b, len);
    SolidNumber* result = solid_exact_add(a, neg_b_solid);
    solid_dec_ref(neg_b_solid);
    
    return result;
}

// Exact multiplication
SolidNumber* solid_exact_multiply(SolidNumber* a, SolidNumber* b) {
    print_str("[SOLID-EXACT] Multiplying exact numbers\n");
    
    // Extract digits
    const char* a_digits = solid_get_known_digits(a);
    const char* b_digits = solid_get_known_digits(b);
    
    // Convert to BigInt
    BigInt* bi_a = string_to_bigint(a_digits, a->known_len);
    BigInt* bi_b = string_to_bigint(b_digits, b->known_len);
    
    // Multiply
    BigInt* result = bigint_multiply(bi_a, bi_b);
    
    // Convert back to string
    char buffer[512];
    uint32_t len;
    bigint_to_string(result, buffer, &len);
    
    return solid_init_exact(buffer, len);
}

// Check if a solid number can be exact
bool solid_can_be_exact(SolidNumber* s) {
    return s && (s->barrier_type == BARRIER_EXACT || 
                (s->barrier_type == BARRIER_COMPUTATIONAL && s->gap_magnitude == 0));
}

// Promote to exact if possible
SolidNumber* solid_to_exact(SolidNumber* s) {
    if (!s) return NULL;
    
    if (s->barrier_type == BARRIER_EXACT) {
        solid_inc_ref(s);
        return s;
    }
    
    if (s->barrier_type == BARRIER_COMPUTATIONAL && s->gap_magnitude < 1000) {
        // Can be made exact
        return solid_init_exact(solid_get_known_digits(s), s->known_len);
    }
    
    // Cannot be made exact
    return NULL;
}

// Exact number validation
bool solid_validate_exact(SolidNumber* s) {
    if (!s || s->barrier_type != BARRIER_EXACT) {
        return false;
    }
    
    // Check that all characters are valid digits or signs
    const char* digits = solid_get_known_digits(s);
    bool has_decimal = false;
    
    for (uint16_t i = 0; i < s->known_len; i++) {
        char c = digits[i];
        
        if (i == 0 && c == '-') {
            continue;  // Leading minus is OK
        }
        
        if (c == '.') {
            if (has_decimal) return false;  // Multiple decimals
            has_decimal = true;
            continue;
        }
        
        if (c < '0' || c > '9') {
            return false;  // Invalid character
        }
    }
    
    return true;
}

// Exact division (returns NULL if not exact)
SolidNumber* solid_exact_divide(SolidNumber* a, SolidNumber* b) {
    print_str("[SOLID-EXACT] Attempting exact division\n");
    
    // For now, only handle simple integer division
    double val_a = solid_to_double(a);
    double val_b = solid_to_double(b);
    
    if (val_b == 0.0) {
        print_str("[SOLID-EXACT] Division by zero\n");
        return NULL;
    }
    
    double result = val_a / val_b;
    
    // Check if result is exact (integer)
    if (result == (double)(int64_t)result) {
        char buffer[32];
        int len = 0;
        int64_t int_result = (int64_t)result;
        
        if (int_result < 0) {
            buffer[len++] = '-';
            int_result = -int_result;
        }
        
        if (int_result == 0) {
            buffer[len++] = '0';
        } else {
            char temp[20];
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
    }
    
    // Not exact - return NULL
    print_str("[SOLID-EXACT] Result is not exact\n");
    return NULL;
}