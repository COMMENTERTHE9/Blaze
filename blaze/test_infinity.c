#include "include/blaze_internals.h"
#include "include/solid_runtime.h"

// Forward declarations
SolidNumber* solid_infinity_divide(SolidNumber* a, SolidNumber* b);
SolidNumber* solid_infinity_power(SolidNumber* base, SolidNumber* exponent);
int solid_infinity_compare(SolidNumber* a, SolidNumber* b);
bool solid_is_infinity(SolidNumber* s);
SolidNumber* solid_positive_infinity(void);
SolidNumber* solid_negative_infinity(void);
SolidNumber* solid_countable_infinity(void);
SolidNumber* solid_continuum_infinity(void);

// Helper function
char barrier_type_char(BarrierType type);
uint16_t combine_confidence(uint16_t a, uint16_t b, char op);

int main() {
    print_str("=== SOLID NUMBER INFINITY ARITHMETIC TEST ===\n\n");
    
    // Initialize solid number pool
    solid_pool_init();
    
    // Test 1: Basic infinities
    print_str("Test 1: Creating different types of infinity\n");
    SolidNumber* pos_inf = solid_positive_infinity();
    SolidNumber* neg_inf = solid_negative_infinity();
    SolidNumber* aleph_0 = solid_countable_infinity();
    SolidNumber* aleph_1 = solid_continuum_infinity();
    
    print_str("Positive infinity: ");
    solid_print(pos_inf);
    print_str("\nNegative infinity: ");
    solid_print(neg_inf);
    print_str("\nCountable infinity (ℵ₀): ");
    solid_print(aleph_0);
    print_str("\nContinuum infinity (ℵ₁): ");
    solid_print(aleph_1);
    print_str("\n\n");
    
    // Test 2: ∞ + ∞
    print_str("Test 2: ∞ + ∞\n");
    SolidNumber* sum = solid_add(pos_inf, pos_inf);
    print_str("Result: ");
    solid_print(sum);
    print_str("\n\n");
    
    // Test 3: ∞ - ∞ (should give natural numbers)
    print_str("Test 3: ∞ - ∞\n");
    SolidNumber* diff = solid_subtract(pos_inf, pos_inf);
    print_str("Result: ");
    solid_print(diff);
    print_str(" (Natural numbers)\n\n");
    
    // Test 4: ∞ × ∞
    print_str("Test 4: ∞ × ∞\n");
    SolidNumber* prod = solid_multiply(pos_inf, pos_inf);
    print_str("Result: ");
    solid_print(prod);
    print_str("\n\n");
    
    // Test 5: ∞ ÷ ∞ with terminal digits
    print_str("Test 5: ∞ ÷ ∞ with terminal digits\n");
    
    // Create infinities with specific terminal patterns
    SolidNumber* inf_a = solid_init_with_gap("", 0, BARRIER_INFINITY, ~0ULL, 900,
                                            "31415", 5, TERMINAL_DIGITS);
    SolidNumber* inf_b = solid_init_with_gap("", 0, BARRIER_INFINITY, ~0ULL, 900,
                                            "27182", 5, TERMINAL_DIGITS);
    
    print_str("Dividend: ");
    solid_print(inf_a);
    print_str("\nDivisor: ");
    solid_print(inf_b);
    print_str("\n");
    
    SolidNumber* quot = solid_infinity_divide(inf_a, inf_b);
    print_str("Result: ");
    solid_print(quot);
    print_str("\n\n");
    
    // Test 6: ∞^∞
    print_str("Test 6: ∞^∞\n");
    SolidNumber* power = solid_infinity_power(pos_inf, pos_inf);
    print_str("Result: ");
    solid_print(power);
    print_str("\n\n");
    
    // Test 7: Finite ÷ ∞
    print_str("Test 7: 42 ÷ ∞\n");
    SolidNumber* finite = solid_init_exact("42", 2);
    SolidNumber* zero_limit = solid_divide(finite, pos_inf);
    print_str("Result: ");
    solid_print(zero_limit);
    print_str("\n\n");
    
    // Test 8: ∞ ÷ finite
    print_str("Test 8: ∞ ÷ 42\n");
    SolidNumber* inf_result = solid_divide(pos_inf, finite);
    print_str("Result: ");
    solid_print(inf_result);
    print_str("\n\n");
    
    // Test 9: Infinity comparison
    print_str("Test 9: Comparing infinities\n");
    int cmp1 = solid_infinity_compare(aleph_0, aleph_1);
    int cmp2 = solid_infinity_compare(pos_inf, finite);
    int cmp3 = solid_infinity_compare(inf_a, inf_b);
    
    print_str("ℵ₀ vs ℵ₁: ");
    print_num(cmp1);
    print_str("\n∞ vs 42: ");
    print_num(cmp2);
    print_str("\n∞(31415) vs ∞(27182): ");
    print_num(cmp3);
    print_str("\n\n");
    
    // Test 10: Powers with infinity
    print_str("Test 10: Various powers with infinity\n");
    
    SolidNumber* two = solid_init_exact("2", 1);
    SolidNumber* one = solid_init_exact("1", 1);
    SolidNumber* half = solid_init_with_gap("0.5", 3, BARRIER_EXACT, 0, 1000,
                                           NULL, 0, TERMINAL_DIGITS);
    
    SolidNumber* two_inf = solid_infinity_power(two, pos_inf);
    print_str("2^∞ = ");
    solid_print(two_inf);
    print_str("\n");
    
    SolidNumber* one_inf = solid_infinity_power(one, pos_inf);
    print_str("1^∞ = ");
    solid_print(one_inf);
    print_str("\n");
    
    SolidNumber* half_inf = solid_infinity_power(half, pos_inf);
    print_str("0.5^∞ = ");
    solid_print(half_inf);
    print_str("\n");
    
    // Clean up
    solid_dec_ref(pos_inf);
    solid_dec_ref(neg_inf);
    solid_dec_ref(aleph_0);
    solid_dec_ref(aleph_1);
    solid_dec_ref(sum);
    solid_dec_ref(diff);
    solid_dec_ref(prod);
    solid_dec_ref(inf_a);
    solid_dec_ref(inf_b);
    solid_dec_ref(quot);
    solid_dec_ref(power);
    solid_dec_ref(finite);
    solid_dec_ref(zero_limit);
    solid_dec_ref(inf_result);
    solid_dec_ref(two);
    solid_dec_ref(one);
    solid_dec_ref(half);
    solid_dec_ref(two_inf);
    solid_dec_ref(one_inf);
    solid_dec_ref(half_inf);
    
    print_str("\n=== ALL INFINITY TESTS COMPLETE ===\n");
    return 0;
}