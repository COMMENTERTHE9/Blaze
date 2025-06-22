// Standalone lexer test - avoid type conflicts
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Include only what we need from blaze
#define MAX_TOKENS 4096

// Copy minimal token types we need
typedef enum {
    TOK_SOLID_NUMBER = 161,
    TOK_VAR = 95,
    TOK_DOT = 130,
    TOK_EQUALS = 135,
    TOK_IDENTIFIER = 114,
    TOK_EOF = 261,
    TOK_ERROR = 262
} TokenType;

typedef struct {
    TokenType type;
    uint32_t start;
    uint16_t len;
    uint16_t line;
} Token;

// External functions from lexer
extern uint32_t lex_blaze(const char* input, uint32_t len, Token* output);
extern const char* token_type_name(TokenType type);

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
    
    printf("=== SOLID NUMBER LEXER TEST ===\n");
    printf("Total tokens: %u\n\n", count);
    
    for (uint32_t i = 0; i < count; i++) {
        Token* tok = &tokens[i];
        if (tok->type == TOK_EOF) break;
        
        const char* type_name = "UNKNOWN";
        switch(tok->type) {
            case TOK_SOLID_NUMBER: type_name = "SOLID_NUMBER"; break;
            case TOK_VAR: type_name = "VAR"; break;
            case TOK_DOT: type_name = "DOT"; break;
            case TOK_EQUALS: type_name = "EQUALS"; break;
            case TOK_IDENTIFIER: type_name = "IDENTIFIER"; break;
            case TOK_ERROR: type_name = "ERROR"; break;
            default: type_name = token_type_name(tok->type); break;
        }
        
        printf("Token %3u: %-20s", i, type_name);
        
        // Extract token text
        if (tok->len > 0 && tok->len < 200) {
            char text[201];
            memcpy(text, source + tok->start, tok->len);
            text[tok->len] = '\0';
            
            // Show the actual text
            printf(" [%s]", text);
        }
        
        printf(" (pos:%u, len:%u)\n", tok->start, tok->len);
        
        // Special handling for solid numbers - show details
        if (tok->type == TOK_SOLID_NUMBER) {
            printf("        -> Solid number found!\n");
        }
    }
    
    free(source);
    return 0;
}