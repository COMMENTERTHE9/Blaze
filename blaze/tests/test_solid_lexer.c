#include "../include/blaze_internals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
uint32_t lex_blaze(const char* input, uint32_t len, Token* output);
const char* token_type_name(TokenType type);

// Simple file reader
char* read_file(const char* filename, size_t* size) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        printf("Error: Cannot open file %s\n", filename);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buffer = malloc(*size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }
    
    fread(buffer, 1, *size, f);
    buffer[*size] = '\0';
    fclose(f);
    
    return buffer;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <blaze_file>\n", argv[0]);
        return 1;
    }
    
    size_t size;
    char* source = read_file(argv[1], &size);
    if (!source) {
        return 1;
    }
    
    Token tokens[MAX_TOKENS];
    uint32_t count = lex_blaze(source, size, tokens);
    
    printf("=== LEXER OUTPUT ===\n");
    printf("Total tokens: %u\n\n", count);
    
    for (uint32_t i = 0; i < count; i++) {
        Token* tok = &tokens[i];
        if (tok->type == TOK_EOF) break;
        
        printf("Token %3u: %-20s", i, token_type_name(tok->type));
        
        // Extract token text
        if (tok->len > 0 && tok->len < 100) {
            char text[101];
            memcpy(text, source + tok->start, tok->len);
            text[tok->len] = '\0';
            
            // Escape special characters for display
            printf(" [");
            for (int j = 0; j < tok->len && j < 50; j++) {
                char c = text[j];
                if (c >= 32 && c <= 126) {
                    printf("%c", c);
                } else if (c == '\n') {
                    printf("\\n");
                } else {
                    printf("\\x%02x", (unsigned char)c);
                }
            }
            if (tok->len > 50) printf("...");
            printf("]");
        }
        
        printf(" (pos:%u, len:%u)\n", tok->start, tok->len);
    }
    
    free(source);
    return 0;
}