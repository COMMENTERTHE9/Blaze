// Debug lexer test program
#include "blaze_internals.h"

// External function from lexer
extern void debug_print_tokens(Token* tokens, uint32_t count, const char* source);

int main(int argc, char** argv) {
    if (argc < 2) {
        print_str("Usage: debug_lexer <file.blaze>\n");
        return 1;
    }
    
    // Read file
    int fd = syscall_open(argv[1], O_RDONLY, 0);
    if (fd < 0) {
        print_str("Error: Cannot open file\n");
        return 1;
    }
    
    char source[32768];
    int bytes_read = syscall6(SYS_READ, fd, (long)source, 32768, 0, 0, 0);
    syscall_close(fd);
    
    if (bytes_read <= 0) {
        print_str("Error: Cannot read file\n");
        return 1;
    }
    
    // Lex the source
    Token tokens[MAX_TOKENS];
    uint32_t token_count = lex_blaze(source, bytes_read, tokens);
    
    print_str("Lexed ");
    print_num(token_count);
    print_str(" tokens\n");
    
    // Print tokens
    debug_print_tokens(tokens, token_count, source);
    
    return 0;
}