#include "include/blaze_internals.h"

// External function from lexer
extern uint32_t lex_blaze(const char* input, uint32_t len, Token* output);
extern const char* token_type_name(TokenType type);

int main() {
    print_str("=== SOLID NUMBER LEXER ERROR HANDLING TEST ===\n\n");
    
    Token tokens[MAX_TOKENS];
    
    // Test cases for malformed solid numbers
    const char* test_cases[] = {
        // Valid solid numbers (should succeed)
        "3.14...(q:10^35|0.85)...926",
        "42...(exact)...42",
        "...(inf:inf)...{*}",
        "2.718...(e:10^20)...null",
        
        // Invalid barrier type
        "3.14...(x:10^5)...926",  // 'x' is not a valid barrier type (except in "exact")
        "3.14...(z:10^5)...926",  // 'z' is not a valid barrier type
        
        // Missing colon
        "3.14...(q10^5)...926",
        
        // Missing gap magnitude
        "3.14...(q:)...926",
        
        // Invalid gap magnitude
        "3.14...(q:abc)...926",
        "3.14...(q:20)...926",  // Must be 10^n or infinity
        
        // Missing closing parenthesis
        "3.14...(q:10^5...926",
        
        // Missing second "..."
        "3.14...(q:10^5)926",
        
        // Missing opening parenthesis
        "3.14...q:10^5)...926",
        
        // Incomplete solid number
        "3.14...",
        "3.14...(q",
        "3.14...(q:",
        "3.14...(q:10^5",
        
        NULL
    };
    
    for (int i = 0; test_cases[i] != NULL; i++) {
        print_str("\nTest case ");
        print_num(i + 1);
        print_str(": \"");
        print_str(test_cases[i]);
        print_str("\"\n");
        
        uint32_t len = 0;
        while (test_cases[i][len] != '\0') len++;
        
        uint32_t token_count = lex_blaze(test_cases[i], len, tokens);
        
        for (uint32_t j = 0; j < token_count; j++) {
            print_str("  Token ");
            print_num(j);
            print_str(": ");
            print_str(token_type_name(tokens[j].type));
            
            if (tokens[j].type == TOK_ERROR) {
                print_str(" [ERROR DETECTED]");
            } else if (tokens[j].type == TOK_SOLID_NUMBER) {
                print_str(" [VALID SOLID NUMBER]");
            }
            
            print_str(" (");
            for (uint32_t k = 0; k < tokens[j].len && k < 20; k++) {
                char c = test_cases[i][tokens[j].start + k];
                if (c >= 32 && c <= 126) {
                    char buf[2] = {c, '\0'};
                    print_str(buf);
                } else {
                    print_str("?");
                }
            }
            if (tokens[j].len > 20) print_str("...");
            print_str(")\n");
        }
    }
    
    print_str("\n=== LEXER ERROR HANDLING TEST COMPLETE ===\n");
    return 0;
}