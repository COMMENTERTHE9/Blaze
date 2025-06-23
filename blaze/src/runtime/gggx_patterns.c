// GGGX PATTERN DETECTION ENGINE
// Advanced algorithms for detecting mathematical patterns in digit sequences

#include "blaze_internals.h"
#include "solid_runtime.h"
#include "gggx.h"

// Pattern types we can detect
typedef enum {
    PATTERN_NONE = 0,
    PATTERN_REPEATING,      // 0.333333...
    PATTERN_CYCLIC,         // 0.142857142857...
    PATTERN_FIBONACCI,      // Fibonacci-based sequences
    PATTERN_PRIME,          // Prime-based patterns
    PATTERN_ALGEBRAIC,      // Algebraic number patterns
    PATTERN_TRANSCENDENTAL, // Transcendental patterns
    PATTERN_CHAOTIC,        // Chaotic/random patterns
    PATTERN_FRACTAL         // Self-similar patterns
} PatternType;

// Advanced pattern detection result
typedef struct {
    PatternType type;
    uint32_t period;
    uint32_t offset;
    double confidence;
    char pattern_desc[256];
} PatternAnalysis;

// Digit frequency analysis
typedef struct {
    uint32_t digit_count[10];
    double entropy;
    double chi_squared;
    bool is_uniform;
} DigitStats;

// Calculate digit statistics
static void analyze_digit_stats(const char* digits, uint32_t len, DigitStats* stats) {
    // Clear counts
    for (int i = 0; i < 10; i++) {
        stats->digit_count[i] = 0;
    }
    
    // Count digits
    uint32_t total = 0;
    for (uint32_t i = 0; i < len; i++) {
        if (digits[i] >= '0' && digits[i] <= '9') {
            stats->digit_count[digits[i] - '0']++;
            total++;
        }
    }
    
    // Calculate entropy
    stats->entropy = 0.0;
    for (int i = 0; i < 10; i++) {
        if (stats->digit_count[i] > 0) {
            double p = (double)stats->digit_count[i] / total;
            stats->entropy -= p * log(p) / log(2.0);
        }
    }
    
    // Chi-squared test for uniformity
    double expected = (double)total / 10;
    stats->chi_squared = 0.0;
    for (int i = 0; i < 10; i++) {
        double diff = stats->digit_count[i] - expected;
        stats->chi_squared += (diff * diff) / expected;
    }
    
    // Critical value for chi-squared with 9 degrees of freedom at 95% confidence
    stats->is_uniform = stats->chi_squared < 16.919;
}

// Check for Fibonacci-like sequences
static bool check_fibonacci_pattern(const char* digits, uint32_t len) {
    if (len < 10) return false;
    
    // Convert first few digits to check Fibonacci property
    int seq[10];
    int seq_len = 0;
    
    for (uint32_t i = 0; i < len && seq_len < 10; i++) {
        if (digits[i] >= '0' && digits[i] <= '9') {
            seq[seq_len++] = digits[i] - '0';
        }
    }
    
    if (seq_len < 6) return false;
    
    // Check if each digit is sum of previous two (mod 10)
    int matches = 0;
    for (int i = 2; i < seq_len; i++) {
        if (seq[i] == (seq[i-1] + seq[i-2]) % 10) {
            matches++;
        }
    }
    
    return matches > seq_len / 2;
}

// Check for prime-based patterns
static bool check_prime_pattern(const char* digits, uint32_t len) {
    // Simple prime list for checking
    static const int primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47};
    static const int num_primes = sizeof(primes) / sizeof(primes[0]);
    
    // Check if digits follow prime sequence or prime gaps
    int matches = 0;
    int digit_val = 0;
    
    for (uint32_t i = 0; i < len && i < 15; i++) {
        if (digits[i] >= '0' && digits[i] <= '9') {
            digit_val = digit_val * 10 + (digits[i] - '0');
            
            // Check if current value is prime
            for (int j = 0; j < num_primes; j++) {
                if (digit_val == primes[j]) {
                    matches++;
                    break;
                }
            }
            
            // Keep digit_val reasonable
            if (digit_val > 100) digit_val = digit_val % 100;
        }
    }
    
    return matches > 2;
}

// Advanced repeating pattern detection with KMP algorithm
static bool kmp_pattern_search(const char* text, uint32_t text_len,
                              const char* pattern, uint32_t pattern_len,
                              uint32_t* occurrences) {
    if (pattern_len == 0 || pattern_len > text_len) return false;
    
    // Build failure function
    int failure[256];
    failure[0] = -1;
    
    for (uint32_t i = 1; i < pattern_len; i++) {
        int j = failure[i - 1];
        while (j >= 0 && pattern[j + 1] != pattern[i]) {
            j = failure[j];
        }
        if (pattern[j + 1] == pattern[i]) {
            failure[i] = j + 1;
        } else {
            failure[i] = -1;
        }
    }
    
    // Search for pattern
    *occurrences = 0;
    uint32_t i = 0, j = 0;
    
    while (i < text_len) {
        if (text[i] == pattern[j]) {
            i++;
            j++;
            if (j == pattern_len) {
                (*occurrences)++;
                j = failure[j - 1] + 1;
            }
        } else if (j > 0) {
            j = failure[j - 1] + 1;
        } else {
            i++;
        }
    }
    
    return *occurrences > 0;
}

// Detect cyclic patterns (like 1/7 = 0.142857142857...)
static bool detect_cyclic_pattern(const char* digits, uint32_t len,
                                 uint32_t* period_out, uint32_t* offset_out) {
    // Try different period lengths
    for (uint32_t period = 1; period <= len / 3; period++) {
        for (uint32_t offset = 0; offset < len - period * 3; offset++) {
            bool is_cyclic = true;
            
            // Check if pattern repeats at least 3 times
            for (uint32_t rep = 1; rep < 3; rep++) {
                for (uint32_t i = 0; i < period; i++) {
                    if (offset + i + rep * period >= len) {
                        is_cyclic = false;
                        break;
                    }
                    if (digits[offset + i] != digits[offset + i + rep * period]) {
                        is_cyclic = false;
                        break;
                    }
                }
                if (!is_cyclic) break;
            }
            
            if (is_cyclic) {
                *period_out = period;
                *offset_out = offset;
                return true;
            }
        }
    }
    
    return false;
}

// Detect algebraic number patterns
static bool detect_algebraic_pattern(double value, const char* digits, uint32_t len) {
    // Check for square roots
    for (int n = 2; n <= 100; n++) {
        double sqrt_n = value * value;
        if (sqrt_n > n - 0.01 && sqrt_n < n + 0.01) {
            return true;
        }
    }
    
    // Check for cube roots
    for (int n = 2; n <= 50; n++) {
        double cube_n = value * value * value;
        if (cube_n > n - 0.01 && cube_n < n + 0.01) {
            return true;
        }
    }
    
    // Check for solutions to simple polynomials
    // x^2 - x - 1 = 0 (golden ratio)
    double test = value * value - value - 1;
    if (test > -0.01 && test < 0.01) {
        return true;
    }
    
    return false;
}

// Detect fractal/self-similar patterns
static bool detect_fractal_pattern(const char* digits, uint32_t len) {
    if (len < 20) return false;
    
    // Look for self-similar subsequences at different scales
    uint32_t scale1 = len / 4;
    uint32_t scale2 = len / 8;
    
    if (scale2 < 3) return false;
    
    // Compare patterns at different scales
    int similarity_count = 0;
    
    for (uint32_t i = 0; i < scale2; i++) {
        if (digits[i] == digits[i + scale1] ||
            digits[i] == digits[i + scale1 * 2]) {
            similarity_count++;
        }
    }
    
    return similarity_count > scale2 / 2;
}

// Main pattern analysis function
PatternAnalysis* analyze_patterns(const char* digits, uint32_t len, double value) {
    static PatternAnalysis analysis;
    
    // Initialize
    analysis.type = PATTERN_NONE;
    analysis.period = 0;
    analysis.offset = 0;
    analysis.confidence = 0.0;
    analysis.pattern_desc[0] = '\0';
    
    // Get digit statistics
    DigitStats stats;
    analyze_digit_stats(digits, len, &stats);
    
    // Check for repeating single digit
    uint32_t occurrences;
    for (int d = 0; d <= 9; d++) {
        if (stats.digit_count[d] > len * 0.8) {
            analysis.type = PATTERN_REPEATING;
            analysis.period = 1;
            analysis.confidence = (double)stats.digit_count[d] / len;
            sprintf(analysis.pattern_desc, "Repeating digit %d", d);
            return &analysis;
        }
    }
    
    // Check for cyclic patterns
    uint32_t period, offset;
    if (detect_cyclic_pattern(digits, len, &period, &offset)) {
        analysis.type = PATTERN_CYCLIC;
        analysis.period = period;
        analysis.offset = offset;
        analysis.confidence = 0.9;
        sprintf(analysis.pattern_desc, "Cyclic pattern with period %u", period);
        return &analysis;
    }
    
    // Check for Fibonacci pattern
    if (check_fibonacci_pattern(digits, len)) {
        analysis.type = PATTERN_FIBONACCI;
        analysis.confidence = 0.8;
        sprintf(analysis.pattern_desc, "Fibonacci-like sequence");
        return &analysis;
    }
    
    // Check for prime pattern
    if (check_prime_pattern(digits, len)) {
        analysis.type = PATTERN_PRIME;
        analysis.confidence = 0.7;
        sprintf(analysis.pattern_desc, "Prime-based pattern");
        return &analysis;
    }
    
    // Check for algebraic pattern
    if (detect_algebraic_pattern(value, digits, len)) {
        analysis.type = PATTERN_ALGEBRAIC;
        analysis.confidence = 0.85;
        sprintf(analysis.pattern_desc, "Algebraic number (root of polynomial)");
        return &analysis;
    }
    
    // Check for fractal pattern
    if (detect_fractal_pattern(digits, len)) {
        analysis.type = PATTERN_FRACTAL;
        analysis.confidence = 0.6;
        sprintf(analysis.pattern_desc, "Self-similar/fractal pattern");
        return &analysis;
    }
    
    // Check entropy for randomness
    if (stats.entropy > 3.0 && stats.is_uniform) {
        analysis.type = PATTERN_CHAOTIC;
        analysis.confidence = stats.entropy / 3.32;  // Normalized by max entropy
        sprintf(analysis.pattern_desc, "High entropy (%.2f), possibly chaotic", stats.entropy);
        return &analysis;
    }
    
    // Default to transcendental if no pattern found but not random
    if (stats.entropy > 2.5 && !stats.is_uniform) {
        analysis.type = PATTERN_TRANSCENDENTAL;
        analysis.confidence = 0.5;
        sprintf(analysis.pattern_desc, "Possibly transcendental");
    }
    
    return &analysis;
}

// Export pattern type name
const char* pattern_type_name(PatternType type) {
    switch (type) {
        case PATTERN_NONE: return "none";
        case PATTERN_REPEATING: return "repeating";
        case PATTERN_CYCLIC: return "cyclic";
        case PATTERN_FIBONACCI: return "fibonacci";
        case PATTERN_PRIME: return "prime";
        case PATTERN_ALGEBRAIC: return "algebraic";
        case PATTERN_TRANSCENDENTAL: return "transcendental";
        case PATTERN_CHAOTIC: return "chaotic";
        case PATTERN_FRACTAL: return "fractal";
        default: return "unknown";
    }
}