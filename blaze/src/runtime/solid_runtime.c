// SOLID NUMBER RUNTIME IMPLEMENTATION
// Memory management and basic operations for solid numbers

#include "blaze_internals.h"
#include "solid_runtime.h"

// Memory pool for solid numbers (avoid frequent malloc/free)
#define SOLID_POOL_SIZE 256
static SolidNumber solid_pool[SOLID_POOL_SIZE];
static uint8_t solid_pool_bitmap[SOLID_POOL_SIZE / 8];  // 1 bit per slot
static bool pool_initialized = false;

// Helper functions
static inline void set_pool_bit(uint32_t index) {
    solid_pool_bitmap[index / 8] |= (1 << (index % 8));
}

static inline void clear_pool_bit(uint32_t index) {
    solid_pool_bitmap[index / 8] &= ~(1 << (index % 8));
}

static inline bool is_pool_bit_set(uint32_t index) {
    return (solid_pool_bitmap[index / 8] & (1 << (index % 8))) != 0;
}

// Initialize the memory pool
void solid_pool_init(void) {
    if (pool_initialized) return;
    
    // Clear bitmap
    for (int i = 0; i < SOLID_POOL_SIZE / 8; i++) {
        solid_pool_bitmap[i] = 0;
    }
    
    // Initialize all solid numbers in pool
    for (int i = 0; i < SOLID_POOL_SIZE; i++) {
        solid_pool[i].uses_heap = false;
        solid_pool[i].ref_count = 0;
    }
    
    pool_initialized = true;
}

// Allocate a solid number from the pool
SolidNumber* solid_alloc(void) {
    if (!pool_initialized) {
        solid_pool_init();
    }
    
    // Find free slot in pool
    for (uint32_t i = 0; i < SOLID_POOL_SIZE; i++) {
        if (!is_pool_bit_set(i)) {
            set_pool_bit(i);
            SolidNumber* solid = &solid_pool[i];
            
            // Initialize to safe defaults
            solid->flags = 0;
            solid->known_len = 0;
            solid->terminal_len = 0;
            solid->terminal_type = TERMINAL_DIGITS;
            solid->barrier_type = BARRIER_EXACT;
            solid->gap_magnitude = 0;
            solid->confidence_x1000 = 1000;  // 100% confidence
            solid->uses_heap = false;
            solid->ref_count = 1;
            
            // Clear inline digits
            for (int j = 0; j < SOLID_INLINE_DIGITS; j++) {
                solid->digits.inline_digits.known[j] = '\0';
            }
            for (int j = 0; j < SOLID_MAX_TERMINAL_DIGITS; j++) {
                solid->digits.inline_digits.terminal[j] = '\0';
            }
            
            return solid;
        }
    }
    
    // Pool exhausted - in real implementation would fall back to malloc
    print_str("[SOLID] ERROR: Pool exhausted!\n");
    return NULL;
}

// Free a solid number
void solid_free(SolidNumber* solid) {
    if (!solid) return;
    
    // If using heap storage, free it
    if (solid->uses_heap) {
        // In real implementation, would free heap memory
        // For now, we don't actually allocate heap memory
    }
    
    // Check if this is from our pool
    if (solid >= &solid_pool[0] && solid < &solid_pool[SOLID_POOL_SIZE]) {
        uint32_t index = solid - &solid_pool[0];
        clear_pool_bit(index);
        solid->ref_count = 0;
    }
}

// Reference counting
void solid_inc_ref(SolidNumber* solid) {
    if (solid) {
        solid->ref_count++;
    }
}

void solid_dec_ref(SolidNumber* solid) {
    if (solid && solid->ref_count > 0) {
        solid->ref_count--;
        if (solid->ref_count == 0) {
            solid_free(solid);
        }
    }
}

// Initialize from AST node
SolidNumber* solid_init_from_ast(ASTNode* node, char* string_pool) {
    if (!node || node->type != NODE_SOLID) {
        return NULL;
    }
    
    SolidNumber* solid = solid_alloc();
    if (!solid) return NULL;
    
    // Copy known digits
    uint16_t known_len = node->data.solid.known_len;
    if (known_len > SOLID_INLINE_DIGITS) {
        // Would need heap allocation
        known_len = SOLID_INLINE_DIGITS;  // Truncate for now
    }
    
    solid->known_len = known_len;
    for (uint16_t i = 0; i < known_len; i++) {
        solid->digits.inline_digits.known[i] = 
            string_pool[node->data.solid.known_offset + i];
    }
    
    // Set barrier information
    solid->barrier_type = (BarrierType)node->data.solid.barrier_type;
    solid->gap_magnitude = node->data.solid.gap_magnitude;
    solid->confidence_x1000 = node->data.solid.confidence_x1000;
    
    // Copy terminal digits
    solid->terminal_type = node->data.solid.terminal_type;
    if (solid->terminal_type == TERMINAL_DIGITS) {
        uint8_t term_len = node->data.solid.terminal_len;
        if (term_len > SOLID_MAX_TERMINAL_DIGITS) {
            term_len = SOLID_MAX_TERMINAL_DIGITS;  // Truncate
        }
        
        solid->terminal_len = term_len;
        for (uint8_t i = 0; i < term_len; i++) {
            solid->digits.inline_digits.terminal[i] = 
                string_pool[node->data.solid.terminal_offset + i];
        }
    } else {
        solid->terminal_len = 0;  // No actual digits for ∅ or {*}
    }
    
    return solid;
}

// Initialize an exact number (no gap)
SolidNumber* solid_init_exact(const char* digits, uint32_t len) {
    SolidNumber* solid = solid_alloc();
    if (!solid) return NULL;
    
    solid->barrier_type = BARRIER_EXACT;
    solid->gap_magnitude = 0;
    solid->confidence_x1000 = 1000;  // 100%
    solid->terminal_type = TERMINAL_DIGITS;
    solid->terminal_len = 0;
    
    // Copy digits
    if (len > SOLID_INLINE_DIGITS) {
        len = SOLID_INLINE_DIGITS;  // Truncate for now
    }
    
    solid->known_len = len;
    for (uint32_t i = 0; i < len; i++) {
        solid->digits.inline_digits.known[i] = digits[i];
    }
    
    return solid;
}

// Initialize with full specification
SolidNumber* solid_init_with_gap(const char* known, uint32_t known_len,
                                 BarrierType barrier, uint64_t gap_mag,
                                 uint16_t confidence, const char* terminal,
                                 uint32_t terminal_len, TerminalType term_type) {
    SolidNumber* solid = solid_alloc();
    if (!solid) return NULL;
    
    // Set barrier info
    solid->barrier_type = barrier;
    solid->gap_magnitude = gap_mag;
    solid->confidence_x1000 = confidence;
    solid->terminal_type = term_type;
    
    // Copy known digits
    if (known_len > SOLID_INLINE_DIGITS) {
        known_len = SOLID_INLINE_DIGITS;
    }
    solid->known_len = known_len;
    for (uint32_t i = 0; i < known_len; i++) {
        solid->digits.inline_digits.known[i] = known[i];
    }
    
    // Copy terminal digits if applicable
    if (term_type == TERMINAL_DIGITS && terminal) {
        if (terminal_len > SOLID_MAX_TERMINAL_DIGITS) {
            terminal_len = SOLID_MAX_TERMINAL_DIGITS;
        }
        solid->terminal_len = terminal_len;
        for (uint32_t i = 0; i < terminal_len; i++) {
            solid->digits.inline_digits.terminal[i] = terminal[i];
        }
    } else {
        solid->terminal_len = 0;
    }
    
    return solid;
}

// Accessors
const char* solid_get_known_digits(SolidNumber* solid) {
    if (!solid) return "";
    
    if (solid->uses_heap) {
        return solid->digits.heap_digits.known_ptr;
    } else {
        return solid->digits.inline_digits.known;
    }
}

const char* solid_get_terminal_digits(SolidNumber* solid) {
    if (!solid || solid->terminal_type != TERMINAL_DIGITS) return "";
    
    if (solid->uses_heap) {
        return solid->digits.heap_digits.terminal_ptr;
    } else {
        return solid->digits.inline_digits.terminal;
    }
}

bool solid_is_exact(SolidNumber* solid) {
    return solid && solid->barrier_type == BARRIER_EXACT;
}

bool solid_is_infinity(SolidNumber* solid) {
    return solid && (solid->barrier_type == BARRIER_INFINITY || 
                    solid->gap_magnitude == ~0ULL);
}

double solid_confidence(SolidNumber* solid) {
    if (!solid) return 0.0;
    return solid->confidence_x1000 / 1000.0;
}

// Convert to double (best approximation)
double solid_to_double(SolidNumber* solid) {
    if (!solid) return 0.0;
    
    // Parse known digits
    double result = 0.0;
    double multiplier = 1.0;
    bool after_decimal = false;
    double decimal_place = 0.1;
    
    const char* known = solid_get_known_digits(solid);
    for (uint16_t i = 0; i < solid->known_len; i++) {
        if (known[i] == '.') {
            after_decimal = true;
        } else if (known[i] >= '0' && known[i] <= '9') {
            if (after_decimal) {
                result += (known[i] - '0') * decimal_place;
                decimal_place *= 0.1;
            } else {
                result = result * 10.0 + (known[i] - '0');
            }
        }
    }
    
    // For non-exact numbers, could add uncertainty estimate
    return result;
}

// Debug print
void solid_print(SolidNumber* solid) {
    if (!solid) {
        print_str("NULL");
        return;
    }
    
    // Print known digits
    const char* known = solid_get_known_digits(solid);
    for (uint16_t i = 0; i < solid->known_len; i++) {
        char buf[2] = {known[i], '\0'};
        print_str(buf);
    }
    
    if (solid->barrier_type != BARRIER_EXACT) {
        print_str("...(");
        
        // Print barrier type
        char barrier_buf[2] = {solid->barrier_type, '\0'};
        print_str(barrier_buf);
        
        if (solid->barrier_type != 'x') {
            print_str(":");
            
            // Print gap magnitude
            if (solid->gap_magnitude == ~0ULL) {
                print_str("∞");
            } else {
                print_str("10^");
                // Simple log10 approximation
                uint64_t mag = solid->gap_magnitude;
                int exp = 0;
                while (mag >= 10) {
                    mag /= 10;
                    exp++;
                }
                print_num(exp);
            }
            
            print_str("|");
            print_num(solid->confidence_x1000);
            print_str("/1000");
        }
        
        print_str(")...");
        
        // Print terminal
        if (solid->terminal_type == TERMINAL_UNDEFINED) {
            print_str("∅");
        } else if (solid->terminal_type == TERMINAL_SUPERPOSITION) {
            print_str("{*}");
        } else if (solid->terminal_len > 0) {
            const char* terminal = solid_get_terminal_digits(solid);
            for (uint16_t i = 0; i < solid->terminal_len; i++) {
                char buf[2] = {terminal[i], '\0'};
                print_str(buf);
            }
        }
    }
}

// Type promotion functions
SolidNumber* solid_from_int(int64_t value) {
    char buffer[32];
    int len = 0;
    
    // Handle negative
    if (value < 0) {
        buffer[len++] = '-';
        value = -value;
    }
    
    // Convert to string
    if (value == 0) {
        buffer[len++] = '0';
    } else {
        // Reverse order first
        char temp[32];
        int temp_len = 0;
        while (value > 0) {
            temp[temp_len++] = '0' + (value % 10);
            value /= 10;
        }
        // Copy in correct order
        for (int i = temp_len - 1; i >= 0; i--) {
            buffer[len++] = temp[i];
        }
    }
    
    return solid_init_exact(buffer, len);
}

SolidNumber* solid_from_float(double value) {
    // Simple float to string conversion
    char buffer[64];
    int len = 0;
    
    // Handle negative
    if (value < 0) {
        buffer[len++] = '-';
        value = -value;
    }
    
    // Integer part
    int64_t int_part = (int64_t)value;
    double frac_part = value - int_part;
    
    // Convert integer part
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
    
    // Decimal point
    buffer[len++] = '.';
    
    // Fractional part (up to 6 digits)
    for (int i = 0; i < 6 && frac_part > 0; i++) {
        frac_part *= 10;
        int digit = (int)frac_part;
        buffer[len++] = '0' + digit;
        frac_part -= digit;
    }
    
    // For floats, we can't guarantee exactness beyond machine precision
    // So create a solid number with computational barrier
    return solid_init_with_gap(buffer, len, BARRIER_COMPUTATIONAL, 
                              1000000000000000ULL,  // 10^15 (double precision limit)
                              950,  // 95% confidence
                              NULL, 0, TERMINAL_DIGITS);
}

// Cleanup pool
void solid_pool_cleanup(void) {
    // Free any heap-allocated memory
    for (int i = 0; i < SOLID_POOL_SIZE; i++) {
        if (is_pool_bit_set(i) && solid_pool[i].uses_heap) {
            // Would free heap memory here
        }
    }
    
    pool_initialized = false;
}