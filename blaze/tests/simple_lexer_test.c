// Simple lexer test
#include "../include/blaze_internals.h"
#include <stdio.h>

extern uint32_t lex_blaze(const char* input, uint32_t len, Token* output);

int main() {
    const char* code = "var.v-x-[42]";
    Token tokens[10];
    
    uint32_t count = lex_blaze(code, 12, tokens);
    
    printf("Token count: %d\n", count);
    for (uint32_t i = 0; i < count; i++) {
        printf("Token %d: type=%d, start=%d, len=%d, text='", 
               i, tokens[i].type, tokens[i].start, tokens[i].len);
        for (int j = 0; j < tokens[i].len; j++) {
            printf("%c", code[tokens[i].start + j]);
        }
        printf("'\n");
    }
    
    // Check specific token types
    if (count > 0 && tokens[0].type == 17) {
        printf("First token is TOK_VAR (17)\n");
    }
    if (count > 1 && tokens[1].type == 12) {
        printf("Second token is TOK_BRACKET_OPEN (12)\n");
    }
    
    return 0;
}