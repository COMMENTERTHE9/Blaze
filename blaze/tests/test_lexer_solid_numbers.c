#include "../include/blaze_internals.h"

// External function from lexer
extern uint32_t lex_blaze(const char* input, uint32_t len, Token* output);
extern const char* token_type_name(TokenType type);

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

// Helper to run a test case
static void test_solid_number(const char* name, const char* input, 
                             bool expect_solid, int expected_tokens) {
    print_str("\n[TEST] ");
    print_str(name);
    print_str("\n  Input: \"");
    print_str(input);
    print_str("\"\n");
    
    Token tokens[MAX_TOKENS];
    uint32_t len = 0;
    while (input[len] != '\0') len++;
    
    uint32_t token_count = lex_blaze(input, len, tokens);
    
    bool found_solid = false;
    bool found_error = false;
    
    for (uint32_t i = 0; i < token_count; i++) {
        if (tokens[i].type == TOK_SOLID_NUMBER) found_solid = true;
        if (tokens[i].type == TOK_ERROR) found_error = true;
    }
    
    bool pass = true;
    
    // Check if we found solid number when expected
    if (expect_solid && !found_solid) {
        print_str("  FAIL: Expected solid number token but didn't find one\n");
        pass = false;
    } else if (!expect_solid && found_solid) {
        print_str("  FAIL: Found solid number token when not expected\n");
        pass = false;
    }
    
    // Check if we got error when expecting invalid input
    if (!expect_solid && !found_error) {
        print_str("  FAIL: Expected error token for invalid input but didn't find one\n");
        pass = false;
    }
    
    // Check token count
    if (expected_tokens > 0 && token_count != expected_tokens) {
        print_str("  FAIL: Expected ");
        print_num(expected_tokens);
        print_str(" tokens but got ");
        print_num(token_count);
        print_str("\n");
        pass = false;
    }
    
    if (pass) {
        print_str("  PASS\n");
        tests_passed++;
    } else {
        tests_failed++;
        
        // Print token details for debugging
        print_str("  Tokens found:\n");
        for (uint32_t i = 0; i < token_count; i++) {
            print_str("    ");
            print_num(i);
            print_str(": ");
            print_str(token_type_name(tokens[i].type));
            print_str(" (");
            for (uint32_t j = 0; j < tokens[i].len && j < 30; j++) {
                char c = input[tokens[i].start + j];
                if (c >= 32 && c <= 126) {
                    char buf[2] = {c, '\0'};
                    print_str(buf);
                }
            }
            print_str(")\n");
        }
    }
}

int main() {
    print_str("=== SOLID NUMBER LEXER UNIT TESTS ===\n");
    
    // Test 1: Basic solid numbers with all barrier types
    print_str("\n--- Test Group 1: Valid Barrier Types ---");
    test_solid_number("Quantum barrier", "3.14...(q:10^35|0.85)...926", true, 2);
    test_solid_number("Energy barrier", "2.718...(e:10^20)...281", true, 2);
    test_solid_number("Storage barrier", "1.414...(s:10^15)...213", true, 2);
    test_solid_number("Temporal barrier", "1.732...(t:10^12)...050", true, 2);
    test_solid_number("Computational barrier", "2.236...(c:10^30)...067", true, 2);
    test_solid_number("Undefined barrier", "0...(u:10^5)...000", true, 2);
    test_solid_number("Exact number", "42...(exact)...42", true, 2);
    
    // Test 2: Infinity barriers
    print_str("\n--- Test Group 2: Infinity Barriers ---");
    test_solid_number("Infinity UTF-8", "...(∞:∞)...{*}", true, 2);
    test_solid_number("Infinity ASCII", "...(inf:inf)...{*}", true, 2);
    test_solid_number("Mixed infinity", "3.14...(∞:10^50)...159", true, 2);
    test_solid_number("Infinity gap only", "2.718...(e:∞)...281", true, 2);
    
    // Test 3: Terminal variations
    print_str("\n--- Test Group 3: Terminal Variations ---");
    test_solid_number("Superposition terminal", "1.618...(q:10^25)...{*}", true, 2);
    test_solid_number("Empty set UTF-8", "0...(c:10^10)...∅", true, 2);
    test_solid_number("Empty set ASCII", "0...(c:10^10)...null", true, 2);
    test_solid_number("Regular digits", "3.14159...(q:10^100)...26535", true, 2);
    test_solid_number("No terminal digits", "99...(e:10^5)...", true, 2);
    
    // Test 4: Confidence levels
    print_str("\n--- Test Group 4: Confidence Levels ---");
    test_solid_number("High confidence", "2.718...(q:10^20|0.99)...281", true, 2);
    test_solid_number("Medium confidence", "1.414...(e:10^15|0.5)...213", true, 2);
    test_solid_number("Low confidence", "3.14...(t:10^10|0.1)...159", true, 2);
    test_solid_number("No confidence", "1.618...(s:10^25)...033", true, 2);
    
    // Test 5: Gap magnitude variations
    print_str("\n--- Test Group 5: Gap Magnitude Variations ---");
    test_solid_number("Small gap", "42...(q:10^5)...42", true, 2);
    test_solid_number("Medium gap", "3.14...(e:10^50)...159", true, 2);
    test_solid_number("Large gap", "2.718...(c:10^1000)...281", true, 2);
    test_solid_number("10 without exponent", "1.414...(s:10)...213", true, 2);
    
    // Test 6: Edge cases for valid inputs
    print_str("\n--- Test Group 6: Edge Cases (Valid) ---");
    test_solid_number("Minimal solid", "0...(exact)...0", true, 2);
    test_solid_number("No known digits", "...(q:10^20)...123", true, 2);
    test_solid_number("Single known digit", "5...(e:10^10)...5", true, 2);
    test_solid_number("Many known digits", "3.14159265358979...(c:10^100)...323", true, 2);
    test_solid_number("Many terminal digits", "2.718...(t:10^50)...28182845904523536", true, 2);
    
    // Test 7: Invalid solid numbers
    print_str("\n--- Test Group 7: Invalid Solid Numbers ---");
    test_solid_number("Invalid barrier type x", "3.14...(x:10^5)...926", false, 2);
    test_solid_number("Invalid barrier type 1", "3.14...(1:10^5)...926", false, 2);
    test_solid_number("Missing colon", "3.14...(q10^5)...926", false, 2);
    test_solid_number("Missing gap", "3.14...(q:)...926", false, 2);
    test_solid_number("Invalid gap", "3.14...(q:abc)...926", false, 2);
    test_solid_number("Wrong gap format", "3.14...(q:20)...926", false, 2);
    test_solid_number("Missing close paren", "3.14...(q:10^5...926", false, 2);
    test_solid_number("Missing second dots", "3.14...(q:10^5)926", false, 2);
    test_solid_number("Missing open paren", "3.14...q:10^5)...926", false, 2);
    test_solid_number("Incomplete 1", "3.14...", false, 2);
    test_solid_number("Incomplete 2", "3.14...(", false, 2);
    test_solid_number("Incomplete 3", "3.14...(q", false, 2);
    test_solid_number("Incomplete 4", "3.14...(q:", false, 2);
    test_solid_number("Incomplete 5", "3.14...(q:10^5", false, 2);
    
    // Test 8: Mixed with other tokens
    print_str("\n--- Test Group 8: Mixed Token Streams ---");
    test_solid_number("Solid after var", "var.x- 3.14...(q:10^5)...926", true, 3);
    test_solid_number("Solid in expression", "2 + 3.14...(e:10^20)...159", true, 4);
    test_solid_number("Multiple solids", "1...(exact)...1 + 2...(exact)...2", true, 4);
    
    // Test 9: Special number formats
    print_str("\n--- Test Group 9: Special Number Formats ---");
    test_solid_number("Negative known", "-3.14...(q:10^5)...159", true, 3); // minus + solid
    test_solid_number("Zero with gap", "0...(c:10^100)...0", true, 2);
    test_solid_number("Large known part", "123456789...(e:10^50)...987654321", true, 2);
    
    // Test 10: Unicode support
    print_str("\n--- Test Group 10: Unicode Support ---");
    test_solid_number("UTF-8 infinity/empty", "...(∞:∞)...∅", true, 2);
    test_solid_number("Mixed UTF-8/ASCII", "3.14...(∞:10^35)...null", true, 2);
    
    // Summary
    print_str("\n=== TEST SUMMARY ===\n");
    print_str("Tests passed: ");
    print_num(tests_passed);
    print_str("\nTests failed: ");
    print_num(tests_failed);
    print_str("\nTotal tests: ");
    print_num(tests_passed + tests_failed);
    print_str("\n");
    
    if (tests_failed == 0) {
        print_str("\nALL TESTS PASSED! ✓\n");
        return 0;
    } else {
        print_str("\nSOME TESTS FAILED! ✗\n");
        return 1;
    }
}