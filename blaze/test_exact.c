#include "include/blaze_internals.h"
#include "include/solid_runtime.h"

// Forward declarations
SolidNumber* solid_exact_add(SolidNumber* a, SolidNumber* b);
SolidNumber* solid_exact_subtract(SolidNumber* a, SolidNumber* b);
SolidNumber* solid_exact_multiply(SolidNumber* a, SolidNumber* b);
SolidNumber* solid_exact_divide(SolidNumber* a, SolidNumber* b);
bool solid_can_be_exact(SolidNumber* s);
SolidNumber* solid_to_exact(SolidNumber* s);
bool solid_validate_exact(SolidNumber* s);

int main() {
    print_str("=== SOLID NUMBER EXACT ARITHMETIC TEST ===\n\n");
    
    // Initialize solid number pool
    solid_pool_init();
    
    // Test 1: Create exact numbers
    print_str("Test 1: Creating exact numbers\n");
    SolidNumber* exact_42 = solid_init_exact("42", 2);
    SolidNumber* exact_neg_17 = solid_init_exact("-17", 3);
    SolidNumber* exact_large = solid_init_exact("123456789012345678901234567890", 30);
    
    print_str("42 = ");
    solid_print(exact_42);
    print_str("\n-17 = ");
    solid_print(exact_neg_17);
    print_str("\nLarge = ");
    solid_print(exact_large);
    print_str("\n\n");
    
    // Test 2: Exact addition
    print_str("Test 2: Exact addition\n");
    SolidNumber* sum1 = solid_exact_add(exact_42, exact_neg_17);
    print_str("42 + (-17) = ");
    solid_print(sum1);
    print_str("\n");
    
    SolidNumber* sum2 = solid_exact_add(exact_42, exact_42);
    print_str("42 + 42 = ");
    solid_print(sum2);
    print_str("\n");
    
    SolidNumber* big_num1 = solid_init_exact("999999999999999999", 18);
    SolidNumber* big_num2 = solid_init_exact("1", 1);
    SolidNumber* sum3 = solid_exact_add(big_num1, big_num2);
    print_str("999999999999999999 + 1 = ");
    solid_print(sum3);
    print_str("\n\n");
    
    // Test 3: Exact subtraction
    print_str("Test 3: Exact subtraction\n");
    SolidNumber* diff1 = solid_exact_subtract(exact_42, exact_neg_17);
    print_str("42 - (-17) = ");
    solid_print(diff1);
    print_str("\n");
    
    SolidNumber* diff2 = solid_exact_subtract(exact_neg_17, exact_42);
    print_str("-17 - 42 = ");
    solid_print(diff2);
    print_str("\n\n");
    
    // Test 4: Exact multiplication
    print_str("Test 4: Exact multiplication\n");
    SolidNumber* prod1 = solid_exact_multiply(exact_42, exact_neg_17);
    print_str("42 × (-17) = ");
    solid_print(prod1);
    print_str("\n");
    
    SolidNumber* twelve = solid_init_exact("12", 2);
    SolidNumber* eleven = solid_init_exact("11", 2);
    SolidNumber* prod2 = solid_exact_multiply(twelve, eleven);
    print_str("12 × 11 = ");
    solid_print(prod2);
    print_str("\n\n");
    
    // Test 5: Large number multiplication
    print_str("Test 5: Large number multiplication\n");
    SolidNumber* large1 = solid_init_exact("123456789", 9);
    SolidNumber* large2 = solid_init_exact("987654321", 9);
    SolidNumber* large_prod = solid_exact_multiply(large1, large2);
    print_str("123456789 × 987654321 = ");
    solid_print(large_prod);
    print_str("\n\n");
    
    // Test 6: Exact division
    print_str("Test 6: Exact division (when possible)\n");
    SolidNumber* hundred = solid_init_exact("100", 3);
    SolidNumber* four = solid_init_exact("4", 1);
    SolidNumber* quot1 = solid_exact_divide(hundred, four);
    if (quot1) {
        print_str("100 ÷ 4 = ");
        solid_print(quot1);
        print_str("\n");
    } else {
        print_str("100 ÷ 4 = Not exact\n");
    }
    
    SolidNumber* quot2 = solid_exact_divide(hundred, exact_42);
    if (quot2) {
        print_str("100 ÷ 42 = ");
        solid_print(quot2);
        print_str("\n");
    } else {
        print_str("100 ÷ 42 = Not exact (as expected)\n");
    }
    
    SolidNumber* neg_hundred = solid_init_exact("-100", 4);
    SolidNumber* twenty = solid_init_exact("20", 2);
    SolidNumber* quot3 = solid_exact_divide(neg_hundred, twenty);
    if (quot3) {
        print_str("-100 ÷ 20 = ");
        solid_print(quot3);
        print_str("\n");
    }
    print_str("\n");
    
    // Test 7: Validation
    print_str("Test 7: Exact number validation\n");
    print_str("42 is valid exact: ");
    print_str(solid_validate_exact(exact_42) ? "YES" : "NO");
    print_str("\n");
    
    // Test an invalid exact number (with gap)
    SolidNumber* not_exact = solid_init_with_gap("3.14", 4, BARRIER_COMPUTATIONAL, 1000, 950,
                                                NULL, 0, TERMINAL_DIGITS);
    print_str("3.14...(c:10³|950/1000)... is valid exact: ");
    print_str(solid_validate_exact(not_exact) ? "YES" : "NO");
    print_str("\n\n");
    
    // Test 8: Promotion to exact
    print_str("Test 8: Promoting to exact when possible\n");
    SolidNumber* small_gap = solid_init_with_gap("7", 1, BARRIER_COMPUTATIONAL, 10, 999,
                                               NULL, 0, TERMINAL_DIGITS);
    print_str("Number with small gap: ");
    solid_print(small_gap);
    print_str("\n");
    
    SolidNumber* promoted = solid_to_exact(small_gap);
    if (promoted) {
        print_str("Promoted to exact: ");
        solid_print(promoted);
        print_str("\n");
    } else {
        print_str("Cannot promote to exact\n");
    }
    
    // Clean up
    solid_dec_ref(exact_42);
    solid_dec_ref(exact_neg_17);
    solid_dec_ref(exact_large);
    solid_dec_ref(sum1);
    solid_dec_ref(sum2);
    solid_dec_ref(sum3);
    solid_dec_ref(diff1);
    solid_dec_ref(diff2);
    solid_dec_ref(prod1);
    solid_dec_ref(prod2);
    solid_dec_ref(twelve);
    solid_dec_ref(eleven);
    solid_dec_ref(large1);
    solid_dec_ref(large2);
    solid_dec_ref(large_prod);
    solid_dec_ref(hundred);
    solid_dec_ref(four);
    solid_dec_ref(twenty);
    solid_dec_ref(neg_hundred);
    solid_dec_ref(big_num1);
    solid_dec_ref(big_num2);
    if (quot1) solid_dec_ref(quot1);
    if (quot2) solid_dec_ref(quot2);
    if (quot3) solid_dec_ref(quot3);
    solid_dec_ref(not_exact);
    solid_dec_ref(small_gap);
    if (promoted) solid_dec_ref(promoted);
    
    print_str("\n=== ALL EXACT NUMBER TESTS COMPLETE ===\n");
    return 0;
}