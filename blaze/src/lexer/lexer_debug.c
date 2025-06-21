// Debug version of lexer with print statements
#include "blaze_internals.h"
#include <stdio.h>

// Helper function to match strings
static bool match_string(const char* input, uint32_t pos, uint32_t len, const char* pattern) {
    uint32_t i = 0;
    while (pattern[i] != '\0') {
        if (pos + i >= len || input[pos + i] != pattern[i]) {
            return false;
        }
        i++;
    }
    return true;
}

// Parse parameter: /{@param:name}
static uint32_t parse_parameter_debug(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    printf("DEBUG: parse_parameter called at pos=%d\n", pos);
    
    if (pos + 9 < len && match_string(input, pos, len, "/{@param:")) {
        printf("DEBUG: Found /{@param: pattern\n");
        uint32_t start = pos;
        pos += 9; // Skip "/{@param:"
        
        // Find the closing }
        while (pos < len && input[pos] != '}') {
            printf("DEBUG: Checking char '%c' at pos %d\n", input[pos], pos);
            if ((input[pos] < 'a' || input[pos] > 'z') && 
                (input[pos] < 'A' || input[pos] > 'Z') &&
                (input[pos] < '0' || input[pos] > '9') &&
                input[pos] != '_') {
                printf("DEBUG: Invalid character in parameter name\n");
                return 0;
            }
            pos++;
        }
        
        if (pos < len && input[pos] == '}') {
            pos++; // Include the closing }
            tok->type = TOK_PARAM;
            tok->len = pos - start;
            printf("DEBUG: Successfully parsed parameter, type=%d, len=%d\n", tok->type, tok->len);
            return pos;
        }
        printf("DEBUG: No closing } found\n");
    }
    printf("DEBUG: Pattern not matched\n");
    return 0;
}

// Mini lexer for testing
uint32_t lex_debug(const char* input, uint32_t len, Token* output) {
    uint32_t pos = 0;
    uint32_t token_count = 0;
    
    // Skip whitespace
    while (pos < len && (input[pos] == ' ' || input[pos] == '\t' || input[pos] == '\n')) {
        pos++;
    }
    
    if (pos >= len) return 0;
    
    Token* tok = &output[token_count];
    tok->start = pos;
    
    // Try parameter
    uint32_t next_pos = parse_parameter_debug(input, pos, len, tok);
    if (next_pos != 0) {
        printf("DEBUG: Parameter parsed successfully\n");
        pos = next_pos;
        token_count++;
    } else {
        printf("DEBUG: Parameter parse failed, falling back to single char\n");
        // Single character
        tok->type = TOK_BANG; // For testing
        tok->len = 1;
        pos++;
        token_count++;
    }
    
    // EOF token
    output[token_count].type = TOK_EOF;
    output[token_count].start = pos;
    output[token_count].len = 0;
    token_count++;
    
    return token_count;
}

int main() {
    const char* input = "/{@param:x}";
    Token tokens[10];
    
    printf("Testing: '%s'\n", input);
    uint32_t count = lex_debug(input, 11, tokens);
    
    printf("\nResult: %d tokens\n", count);
    for (uint32_t i = 0; i < count; i++) {
        printf("Token %d: type=%d, len=%d\n", i, tokens[i].type, tokens[i].len);
    }
    
    return 0;
}