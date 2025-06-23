// GGGX TERMINAL DIGIT ANALYZER
// Advanced algorithms for extracting and analyzing terminal digits

#include "blaze_internals.h"
#include "solid_runtime.h"
#include "gggx.h"

// Terminal digit extraction methods
typedef enum {
    EXTRACT_MODULAR,      // Modular arithmetic extraction
    EXTRACT_CONTINUED,    // Continued fraction convergents
    EXTRACT_SERIES,       // Series expansion remainders
    EXTRACT_ITERATIVE,    // Iterative algorithm states
    EXTRACT_QUANTUM       // Quantum measurement collapse
} ExtractionMethod;

// Terminal analysis result
typedef struct {
    char digits[SOLID_MAX_TERMINAL_DIGITS];
    uint16_t length;
    TerminalType type;
    ExtractionMethod method;
    double stability;      // How stable the terminals are
    bool has_pattern;
    uint32_t pattern_period;
} TerminalAnalysis;

// Modular arithmetic state
typedef struct {
    uint64_t modulus;
    uint64_t current;
    uint32_t period;
    bool found_period;
} ModularState;

// Continued fraction representation
typedef struct {
    int64_t p[100];  // Numerators
    int64_t q[100];  // Denominators
    int64_t a[100];  // Partial quotients
    uint32_t length;
} ContinuedFraction;

// Extract terminal digits using modular arithmetic
static void extract_modular_terminals(double value, uint64_t gap_magnitude,
                                    TerminalAnalysis* result) {
    ModularState state;
    state.modulus = 1;
    
    // Determine appropriate modulus based on gap
    for (int i = 0; i < SOLID_MAX_TERMINAL_DIGITS && state.modulus < gap_magnitude; i++) {
        state.modulus *= 10;
    }
    
    if (state.modulus > gap_magnitude) {
        state.modulus = gap_magnitude;
    }
    
    // Extract digits after the gap using modular arithmetic
    double scaled = value * state.modulus;
    state.current = (uint64_t)scaled % state.modulus;
    
    // Convert to digits
    result->length = 0;
    uint64_t temp = state.current;
    char temp_digits[SOLID_MAX_TERMINAL_DIGITS];
    
    while (temp > 0 && result->length < SOLID_MAX_TERMINAL_DIGITS) {
        temp_digits[result->length++] = '0' + (temp % 10);
        temp /= 10;
    }
    
    // Reverse the digits
    for (uint16_t i = 0; i < result->length; i++) {
        result->digits[i] = temp_digits[result->length - 1 - i];
    }
    
    result->method = EXTRACT_MODULAR;
    result->stability = 0.8;  // Modular arithmetic is fairly stable
    
    // Check for period in modular sequence
    state.found_period = false;
    state.period = 0;
    
    // Simple period detection
    if (result->length > 3) {
        for (uint32_t p = 1; p <= result->length / 2; p++) {
            bool is_periodic = true;
            for (uint32_t i = 0; i < p && i + p < result->length; i++) {
                if (result->digits[i] != result->digits[i + p]) {
                    is_periodic = false;
                    break;
                }
            }
            if (is_periodic) {
                state.found_period = true;
                state.period = p;
                break;
            }
        }
    }
    
    result->has_pattern = state.found_period;
    result->pattern_period = state.period;
}

// Convert to continued fraction
static void to_continued_fraction(double value, ContinuedFraction* cf) {
    cf->length = 0;
    
    // Initialize
    cf->p[0] = 1;
    cf->q[0] = 0;
    cf->p[1] = (int64_t)value;
    cf->q[1] = 1;
    
    double x = value;
    
    for (uint32_t i = 0; i < 50 && cf->length < 98; i++) {
        int64_t a_i = (int64_t)x;
        cf->a[cf->length++] = a_i;
        
        if (i >= 2) {
            cf->p[i] = a_i * cf->p[i-1] + cf->p[i-2];
            cf->q[i] = a_i * cf->q[i-1] + cf->q[i-2];
        }
        
        x = x - a_i;
        if (x < 0.0000001) break;
        
        x = 1.0 / x;
        
        // Check for period
        if (cf->length > 10) {
            // Look for repeating partial quotients
            for (uint32_t period = 1; period <= cf->length / 2; period++) {
                bool repeats = true;
                for (uint32_t j = 0; j < period; j++) {
                    if (cf->a[cf->length - 1 - j] != cf->a[cf->length - 1 - j - period]) {
                        repeats = false;
                        break;
                    }
                }
                if (repeats) {
                    return;  // Found period
                }
            }
        }
    }
}

// Extract terminals from continued fraction convergents
static void extract_continued_terminals(double value, uint64_t gap_magnitude,
                                      TerminalAnalysis* result) {
    ContinuedFraction cf;
    to_continued_fraction(value, &cf);
    
    if (cf.length < 2) {
        result->length = 0;
        result->type = TERMINAL_UNDEFINED;
        return;
    }
    
    // Use the remainders from continued fraction as terminals
    result->length = 0;
    
    // Take last few partial quotients as terminal pattern
    uint32_t start = cf.length > SOLID_MAX_TERMINAL_DIGITS ? 
                     cf.length - SOLID_MAX_TERMINAL_DIGITS : 0;
    
    for (uint32_t i = start; i < cf.length && result->length < SOLID_MAX_TERMINAL_DIGITS; i++) {
        if (cf.a[i] < 10) {
            result->digits[result->length++] = '0' + cf.a[i];
        } else {
            // Multi-digit partial quotient
            int64_t temp = cf.a[i];
            char temp_buf[20];
            int temp_len = 0;
            
            while (temp > 0 && temp_len < 20) {
                temp_buf[temp_len++] = '0' + (temp % 10);
                temp /= 10;
            }
            
            // Add in reverse
            for (int j = temp_len - 1; j >= 0 && result->length < SOLID_MAX_TERMINAL_DIGITS; j--) {
                result->digits[result->length++] = temp_buf[j];
            }
        }
    }
    
    result->method = EXTRACT_CONTINUED;
    result->stability = 0.9;  // Continued fractions are very stable
    
    // Check for periodic continued fraction
    result->has_pattern = false;
    if (cf.length > 5) {
        for (uint32_t period = 1; period <= cf.length / 2; period++) {
            bool periodic = true;
            for (uint32_t i = 0; i < period; i++) {
                if (cf.a[cf.length - 1 - i] != cf.a[cf.length - 1 - i - period]) {
                    periodic = false;
                    break;
                }
            }
            if (periodic) {
                result->has_pattern = true;
                result->pattern_period = period;
                break;
            }
        }
    }
}

// Extract terminals from series expansion
static void extract_series_terminals(double value, uint64_t gap_magnitude,
                                   TerminalAnalysis* result) {
    // For transcendental numbers, use series expansion remainders
    result->length = 0;
    
    // Example: for e^x, the remainder after n terms gives terminal behavior
    double remainder = value;
    double factorial = 1.0;
    
    for (int n = 1; n < 20 && result->length < SOLID_MAX_TERMINAL_DIGITS; n++) {
        factorial *= n;
        double term = 1.0 / factorial;
        
        if (term < 1.0 / gap_magnitude) {
            // We're in the terminal region
            int digit = (int)(remainder * 10);
            if (digit >= 0 && digit <= 9) {
                result->digits[result->length++] = '0' + digit;
                remainder = remainder * 10 - digit;
            }
        } else {
            remainder -= term;
        }
    }
    
    result->method = EXTRACT_SERIES;
    result->stability = 0.7;  // Series can be less stable
    result->has_pattern = false;  // Series rarely have simple patterns
}

// Extract terminals from iterative algorithms
static void extract_iterative_terminals(double value, uint64_t gap_magnitude,
                                       TerminalAnalysis* result) {
    // Simulate iteration of a dynamical system
    double x = value;
    result->length = 0;
    
    // Use logistic map as example: x_{n+1} = r * x_n * (1 - x_n)
    double r = 3.7;  // Chaotic regime
    
    // Skip to terminal region
    for (int i = 0; i < 100; i++) {
        x = r * x * (1 - x);
    }
    
    // Extract terminal digits
    for (int i = 0; i < SOLID_MAX_TERMINAL_DIGITS && result->length < SOLID_MAX_TERMINAL_DIGITS; i++) {
        x = r * x * (1 - x);
        int digit = (int)(x * 10) % 10;
        result->digits[result->length++] = '0' + digit;
    }
    
    result->method = EXTRACT_ITERATIVE;
    result->stability = 0.3;  // Chaotic systems are unstable
    result->has_pattern = false;  // Chaos typically has no pattern
}

// Quantum measurement simulation
static void extract_quantum_terminals(double value, uint64_t gap_magnitude,
                                    TerminalAnalysis* result) {
    // Simulate quantum measurement collapse
    result->length = 0;
    result->type = TERMINAL_SUPERPOSITION;
    
    // In quantum regime, terminals are in superposition
    // We can only give probability distributions
    for (int i = 0; i < SOLID_MAX_TERMINAL_DIGITS / 2; i++) {
        result->digits[i] = '*';  // Superposition marker
    }
    result->length = SOLID_MAX_TERMINAL_DIGITS / 2;
    
    result->method = EXTRACT_QUANTUM;
    result->stability = 0.1;  // Quantum measurements are probabilistic
    result->has_pattern = false;
}

// Determine best extraction method
static ExtractionMethod choose_extraction_method(double value, BarrierType barrier,
                                                uint64_t gap_magnitude) {
    // Quantum barriers need quantum extraction
    if (barrier == BARRIER_QUANTUM) {
        return EXTRACT_QUANTUM;
    }
    
    // Rational numbers work well with modular
    for (int denom = 2; denom <= 100; denom++) {
        double test = value * denom;
        if (fabs(test - round(test)) < 0.0001) {
            return EXTRACT_MODULAR;
        }
    }
    
    // Algebraic numbers often have nice continued fractions
    double squared = value * value;
    if (fabs(squared - round(squared)) < 0.01) {
        return EXTRACT_CONTINUED;
    }
    
    // Transcendental numbers might use series
    if (fabs(value - 3.14159265359) < 0.001 ||
        fabs(value - 2.71828182846) < 0.001) {
        return EXTRACT_SERIES;
    }
    
    // Chaotic values use iterative
    if (value > 0 && value < 1 && barrier == BARRIER_TEMPORAL) {
        return EXTRACT_ITERATIVE;
    }
    
    // Default to modular
    return EXTRACT_MODULAR;
}

// Main terminal extraction function
TerminalAnalysis* extract_terminal_digits(double value, BarrierType barrier,
                                         uint64_t gap_magnitude) {
    static TerminalAnalysis analysis;
    
    // Initialize
    analysis.length = 0;
    analysis.type = TERMINAL_DIGITS;
    analysis.stability = 0.5;
    analysis.has_pattern = false;
    analysis.pattern_period = 0;
    
    // Choose extraction method
    ExtractionMethod method = choose_extraction_method(value, barrier, gap_magnitude);
    
    print_str("[GGGX-TERMINAL] Using extraction method: ");
    switch (method) {
        case EXTRACT_MODULAR:
            print_str("modular");
            extract_modular_terminals(value, gap_magnitude, &analysis);
            break;
        case EXTRACT_CONTINUED:
            print_str("continued fraction");
            extract_continued_terminals(value, gap_magnitude, &analysis);
            break;
        case EXTRACT_SERIES:
            print_str("series expansion");
            extract_series_terminals(value, gap_magnitude, &analysis);
            break;
        case EXTRACT_ITERATIVE:
            print_str("iterative");
            extract_iterative_terminals(value, gap_magnitude, &analysis);
            break;
        case EXTRACT_QUANTUM:
            print_str("quantum");
            extract_quantum_terminals(value, gap_magnitude, &analysis);
            break;
    }
    print_str("\n");
    
    // Determine terminal type based on extraction results
    if (method == EXTRACT_QUANTUM || analysis.stability < 0.3) {
        analysis.type = TERMINAL_SUPERPOSITION;
    } else if (analysis.length == 0) {
        analysis.type = TERMINAL_UNDEFINED;
    } else {
        analysis.type = TERMINAL_DIGITS;
    }
    
    // Log results
    if (analysis.length > 0 && analysis.type == TERMINAL_DIGITS) {
        print_str("[GGGX-TERMINAL] Extracted ");
        print_num(analysis.length);
        print_str(" terminal digits: ");
        for (uint16_t i = 0; i < analysis.length && i < 10; i++) {
            print_char(analysis.digits[i]);
        }
        if (analysis.length > 10) print_str("...");
        print_str("\n");
    }
    
    return &analysis;
}

// Analyze terminal digit statistics
void analyze_terminal_statistics(TerminalAnalysis* terminals) {
    if (terminals->length == 0 || terminals->type != TERMINAL_DIGITS) {
        return;
    }
    
    // Count digit frequencies
    int digit_count[10] = {0};
    for (uint16_t i = 0; i < terminals->length; i++) {
        if (terminals->digits[i] >= '0' && terminals->digits[i] <= '9') {
            digit_count[terminals->digits[i] - '0']++;
        }
    }
    
    // Calculate chi-squared statistic
    double expected = (double)terminals->length / 10;
    double chi_squared = 0.0;
    
    for (int i = 0; i < 10; i++) {
        double diff = digit_count[i] - expected;
        chi_squared += (diff * diff) / expected;
    }
    
    // Update stability based on uniformity
    if (chi_squared < 16.919) {  // 95% confidence for 9 degrees of freedom
        terminals->stability *= 1.1;  // Increase stability for uniform distribution
    } else {
        terminals->stability *= 0.9;  // Decrease for non-uniform
    }
    
    // Clamp stability
    if (terminals->stability > 1.0) terminals->stability = 1.0;
    if (terminals->stability < 0.1) terminals->stability = 0.1;
}