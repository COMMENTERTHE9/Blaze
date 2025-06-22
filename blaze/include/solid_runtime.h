// SOLID NUMBER RUNTIME STRUCTURES AND FUNCTIONS
// Runtime representation of solid numbers in Blaze

#ifndef SOLID_RUNTIME_H
#define SOLID_RUNTIME_H

#include "blaze_types.h"

// Maximum digits we can store inline (for optimization)
#define SOLID_INLINE_DIGITS 32
#define SOLID_MAX_TERMINAL_DIGITS 16

// Barrier types for solid numbers
typedef enum {
    BARRIER_QUANTUM = 'q',      // Quantum uncertainty
    BARRIER_ENERGY = 'e',       // Energy constraints
    BARRIER_STORAGE = 's',      // Storage limitations
    BARRIER_TEMPORAL = 't',     // Time constraints
    BARRIER_COMPUTATIONAL = 'c', // Computational limits
    BARRIER_INFINITY = 'i',     // Infinity (∞)
    BARRIER_UNDEFINED = 'u',    // Undefined barrier
    BARRIER_EXACT = 'x'         // No barrier (exact number)
} BarrierType;

// Terminal type
typedef enum {
    TERMINAL_DIGITS = 0,        // Regular digits
    TERMINAL_UNDEFINED = 1,     // ∅ (empty set)
    TERMINAL_SUPERPOSITION = 2  // {*} (all possible values)
} TerminalType;

// Runtime representation of a solid number
typedef struct SolidNumber {
    // Header information
    uint16_t flags;             // Various flags (sign, etc.)
    uint16_t known_len;         // Length of known digits
    uint16_t terminal_len;      // Length of terminal digits
    uint8_t terminal_type;      // Type of terminal (digits, ∅, {*})
    BarrierType barrier_type;   // Type of computational barrier
    
    // Gap information
    uint64_t gap_magnitude;     // 10^n or UINT64_MAX for infinity
    uint16_t confidence_x1000;  // Confidence level (0-1000)
    
    // Digit storage
    union {
        // For small numbers, store inline
        struct {
            char known[SOLID_INLINE_DIGITS];
            char terminal[SOLID_MAX_TERMINAL_DIGITS];
        } inline_digits;
        
        // For large numbers, use pointers
        struct {
            char* known_ptr;    // Pointer to known digits
            char* terminal_ptr; // Pointer to terminal digits
            uint32_t known_capacity;
            uint32_t terminal_capacity;
        } heap_digits;
    } digits;
    
    // Memory management
    bool uses_heap;             // True if using heap allocation
    uint32_t ref_count;         // Reference counting for memory management
} SolidNumber;

// Solid number allocation and initialization
SolidNumber* solid_alloc(void);
void solid_free(SolidNumber* solid);
SolidNumber* solid_init_from_ast(ASTNode* node, char* string_pool);
SolidNumber* solid_init_exact(const char* digits, uint32_t len);
SolidNumber* solid_init_with_gap(const char* known, uint32_t known_len,
                                 BarrierType barrier, uint64_t gap_mag,
                                 uint16_t confidence, const char* terminal,
                                 uint32_t terminal_len, TerminalType term_type);

// Reference counting
void solid_inc_ref(SolidNumber* solid);
void solid_dec_ref(SolidNumber* solid);

// Accessors
const char* solid_get_known_digits(SolidNumber* solid);
const char* solid_get_terminal_digits(SolidNumber* solid);
bool solid_is_exact(SolidNumber* solid);
bool solid_is_infinity(SolidNumber* solid);
double solid_confidence(SolidNumber* solid);

// Conversion functions
double solid_to_double(SolidNumber* solid);  // Best approximation
char* solid_to_string(SolidNumber* solid);   // Full representation
void solid_print(SolidNumber* solid);        // Debug print

// Basic operations (to be implemented in Phase 5)
SolidNumber* solid_add(SolidNumber* a, SolidNumber* b);
SolidNumber* solid_subtract(SolidNumber* a, SolidNumber* b);
SolidNumber* solid_multiply(SolidNumber* a, SolidNumber* b);
SolidNumber* solid_divide(SolidNumber* a, SolidNumber* b);

// Type promotion
SolidNumber* solid_from_int(int64_t value);
SolidNumber* solid_from_float(double value);

// Memory pool for efficient allocation
void solid_pool_init(void);
void solid_pool_cleanup(void);

#endif // SOLID_RUNTIME_H