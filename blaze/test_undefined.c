#include "include/blaze_internals.h"
#include "include/solid_runtime.h"

// Forward declarations
SolidNumber* solid_undefined_with_reason(int reason, const char* details,
                                       SolidNumber* a, SolidNumber* b, char op);
bool solid_would_be_undefined(SolidNumber* a, SolidNumber* b, char op);
SolidNumber* solid_undefined_add(SolidNumber* a, SolidNumber* b);
SolidNumber* solid_undefined_multiply(SolidNumber* a, SolidNumber* b);
SolidNumber* solid_undefined_divide(SolidNumber* a, SolidNumber* b);
SolidNumber* solid_undefined_power(SolidNumber* base, SolidNumber* exp);
SolidNumber* solid_sqrt(SolidNumber* x);
SolidNumber* solid_log(SolidNumber* x);
bool solid_is_zero(SolidNumber* s);
bool solid_is_negative(SolidNumber* s);
bool solid_is_integer(SolidNumber* s);
const char* solid_undefined_reason(SolidNumber* s);
SolidNumber* solid_recover_from_undefined(SolidNumber* undef, RecoveryStrategy strategy);

// Undefined reason codes
#define UNDEFINED_DIVISION_BY_ZERO 0
#define UNDEFINED_ZERO_TO_ZERO 1
#define UNDEFINED_ZERO_TIMES_INFINITY 3
#define UNDEFINED_SQRT_NEGATIVE 4

int main() {
    print_str("=== SOLID NUMBER UNDEFINED HANDLING TEST ===\n\n");
    
    // Initialize solid number pool
    solid_pool_init();
    
    // Test 1: Division by zero
    print_str("Test 1: Division by zero\n");
    SolidNumber* forty_two = solid_init_exact("42", 2);
    SolidNumber* zero = solid_init_exact("0", 1);
    
    print_str("Checking if 42 ÷ 0 would be undefined: ");
    print_str(solid_would_be_undefined(forty_two, zero, '/') ? "YES" : "NO");
    print_str("\n");
    
    SolidNumber* div_by_zero = solid_undefined_divide(forty_two, zero);
    if (div_by_zero) {
        print_str("Result: ");
        solid_print(div_by_zero);
        print_str("\n");
    }
    print_str("\n");
    
    // Test 2: 0^0 indeterminate form
    print_str("Test 2: 0^0 indeterminate form\n");
    print_str("Checking if 0^0 would be undefined: ");
    print_str(solid_would_be_undefined(zero, zero, '^') ? "YES" : "NO");
    print_str("\n");
    
    SolidNumber* zero_to_zero = solid_undefined_power(zero, zero);
    if (zero_to_zero) {
        print_str("Result: ");
        solid_print(zero_to_zero);
        print_str("\n");
    }
    print_str("\n");
    
    // Test 3: 0 × ∞
    print_str("Test 3: 0 × ∞ indeterminate form\n");
    SolidNumber* infinity = solid_init_with_gap("", 0, BARRIER_INFINITY, ~0ULL, 1000,
                                               NULL, 0, TERMINAL_UNDEFINED);
    
    print_str("Checking if 0 × ∞ would be undefined: ");
    print_str(solid_would_be_undefined(zero, infinity, '*') ? "YES" : "NO");
    print_str("\n");
    
    SolidNumber* zero_times_inf = solid_undefined_multiply(zero, infinity);
    if (zero_times_inf) {
        print_str("Result: ");
        solid_print(zero_times_inf);
        print_str("\n");
    }
    print_str("\n");
    
    // Test 4: Square root of negative
    print_str("Test 4: Square root of negative number\n");
    SolidNumber* neg_four = solid_init_exact("-4", 2);
    
    SolidNumber* sqrt_neg = solid_sqrt(neg_four);
    print_str("√(-4) = ");
    solid_print(sqrt_neg);
    print_str("\n\n");
    
    // Test 5: Square root of positive (should work)
    print_str("Test 5: Square root of positive number\n");
    SolidNumber* four = solid_init_exact("4", 1);
    SolidNumber* sqrt_pos = solid_sqrt(four);
    print_str("√4 = ");
    solid_print(sqrt_pos);
    print_str("\n\n");
    
    // Test 6: Undefined propagation
    print_str("Test 6: Undefined propagation\n");
    SolidNumber* undef = solid_init_with_gap("", 0, BARRIER_UNDEFINED, 0, 0,
                                            NULL, 0, TERMINAL_UNDEFINED);
    
    SolidNumber* sum_with_undef = solid_add(forty_two, undef);
    print_str("42 + undefined = ");
    solid_print(sum_with_undef);
    print_str("\n");
    
    SolidNumber* prod_with_undef = solid_multiply(forty_two, undef);
    print_str("42 × undefined = ");
    solid_print(prod_with_undef);
    print_str("\n\n");
    
    // Test 7: Property checks
    print_str("Test 7: Property checks\n");
    print_str("Is 0 zero? ");
    print_str(solid_is_zero(zero) ? "YES" : "NO");
    print_str("\n");
    
    print_str("Is 42 zero? ");
    print_str(solid_is_zero(forty_two) ? "YES" : "NO");
    print_str("\n");
    
    print_str("Is -4 negative? ");
    print_str(solid_is_negative(neg_four) ? "YES" : "NO");
    print_str("\n");
    
    print_str("Is 42 negative? ");
    print_str(solid_is_negative(forty_two) ? "YES" : "NO");
    print_str("\n");
    
    print_str("Is 42 an integer? ");
    print_str(solid_is_integer(forty_two) ? "YES" : "NO");
    print_str("\n");
    
    SolidNumber* pi_ish = solid_init_exact("3.14159", 7);
    print_str("Is 3.14159 an integer? ");
    print_str(solid_is_integer(pi_ish) ? "YES" : "NO");
    print_str("\n\n");
    
    // Test 8: Recovery strategies
    print_str("Test 8: Recovery strategies from undefined\n");
    
    SolidNumber* recovered_zero = solid_recover_from_undefined(div_by_zero, RECOVERY_USE_ZERO);
    print_str("Recover with zero: ");
    solid_print(recovered_zero);
    print_str("\n");
    
    SolidNumber* recovered_one = solid_recover_from_undefined(div_by_zero, RECOVERY_USE_ONE);
    print_str("Recover with one: ");
    solid_print(recovered_one);
    print_str("\n");
    
    SolidNumber* recovered_inf = solid_recover_from_undefined(div_by_zero, RECOVERY_USE_INFINITY);
    print_str("Recover with infinity: ");
    solid_print(recovered_inf);
    print_str("\n");
    
    SolidNumber* recovered_prop = solid_recover_from_undefined(div_by_zero, RECOVERY_PROPAGATE);
    print_str("Recover with propagate: ");
    solid_print(recovered_prop);
    print_str("\n\n");
    
    // Test 9: Check that ∞ - ∞ is NOT undefined
    print_str("Test 9: Verify ∞ - ∞ is NOT undefined\n");
    print_str("Checking if ∞ - ∞ would be undefined: ");
    print_str(solid_would_be_undefined(infinity, infinity, '-') ? "YES" : "NO");
    print_str(" (correct - it equals ℕ)\n");
    
    // Clean up
    solid_dec_ref(forty_two);
    solid_dec_ref(zero);
    solid_dec_ref(infinity);
    solid_dec_ref(neg_four);
    solid_dec_ref(four);
    solid_dec_ref(undef);
    solid_dec_ref(pi_ish);
    if (div_by_zero) solid_dec_ref(div_by_zero);
    if (zero_to_zero) solid_dec_ref(zero_to_zero);
    if (zero_times_inf) solid_dec_ref(zero_times_inf);
    if (sqrt_neg) solid_dec_ref(sqrt_neg);
    if (sqrt_pos) solid_dec_ref(sqrt_pos);
    if (sum_with_undef) solid_dec_ref(sum_with_undef);
    if (prod_with_undef) solid_dec_ref(prod_with_undef);
    if (recovered_zero) solid_dec_ref(recovered_zero);
    if (recovered_one) solid_dec_ref(recovered_one);
    if (recovered_inf) solid_dec_ref(recovered_inf);
    if (recovered_prop) solid_dec_ref(recovered_prop);
    
    print_str("\n=== ALL UNDEFINED HANDLING TESTS COMPLETE ===\n");
    return 0;
}