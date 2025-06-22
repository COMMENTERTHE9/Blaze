// BLAZE LEXER - Direct byte processing, no heap allocation

#include "blaze_internals.h"

// Forward declarations for debug
extern void print_str(const char* str);
// print_num is already defined in blaze_internals.h

// Token buffer - stack allocated
typedef struct {
    Token tokens[MAX_TOKENS];
    uint32_t count;
    uint32_t capacity;
} TokenBuffer;

// Debug function for printing tokens
const char* token_type_name(TokenType type) {
    switch(type) {
        case TOK_PRINT: return "TOK_PRINT";
        case TOK_DIV: return "TOK_DIV";
        case TOK_NUMBER: return "TOK_NUMBER";
        case TOK_BACKSLASH: return "TOK_BACKSLASH";
        case TOK_EOF: return "TOK_EOF";
        case TOK_SLASH: return "TOK_SLASH";
        case TOK_PLUS: return "TOK_PLUS";
        case TOK_MINUS: return "TOK_MINUS";
        case TOK_STAR: return "TOK_STAR";
        default: return "UNKNOWN";
    }
}

void debug_print_tokens(Token* tokens, uint32_t count, const char* source) {
    // Capture register values immediately on entry before any other code
    register uint64_t saved_rdi __asm__("rdi");
    register uint64_t saved_rsi __asm__("rsi");
    register uint64_t saved_rdx __asm__("rdx");
    uint64_t rdi_val = saved_rdi;
    uint64_t rsi_val = saved_rsi;
    uint64_t rdx_val = saved_rdx;
    
    print_str("=== TOKEN DUMP ===\n");
    print_str("DEBUG: Inside debug_print_tokens\n");
    
    print_str("  Register values on entry:\n");
    print_str("    RDI (tokens): ");
    print_num((long)rdi_val);
    print_str("\n");
    print_str("    RSI (count): ");
    print_num((long)rsi_val);
    print_str(" = ");
    print_num((long)(rsi_val & 0xFFFFFFFF));  // Lower 32 bits
    print_str(" (low) + ");
    print_num((long)(rsi_val >> 32));  // Upper 32 bits
    print_str(" (high)\n");
    print_str("    RDX (source): ");
    print_num((long)rdx_val);
    print_str("\n");
    
    // Check stack alignment
    uint64_t rsp_value;
    __asm__ volatile("mov %%rsp, %0" : "=r"(rsp_value));
    print_str("  Stack pointer (RSP): ");
    print_num((long)rsp_value);
    print_str(" (alignment: ");
    print_num((long)(rsp_value & 0xF));
    print_str(")\n");
    
    print_str("  Received count parameter value: ");
    print_num((long)count);
    print_str(" (as uint32_t)\n");
    print_str("  Received count parameter value: ");
    print_num((long)(uint64_t)count);
    print_str(" (as uint64_t)\n");
    print_str("  Received count address: ");
    print_num((long)(unsigned long)&count);
    print_str("\n");
    print_str("  Expected vs actual:\n");
    print_str("    Expected from RSI low 32: ");
    print_num((long)(uint32_t)(rsi_val & 0xFFFFFFFF));
    print_str("\n");
    print_str("    Actual count parameter: ");
    print_num((long)count);
    print_str("\n");
    print_str("  Received tokens address: ");
    print_num((long)(unsigned long)tokens);
    print_str("\n");
    print_str("  Received source address: ");
    print_num((long)(unsigned long)source);
    print_str("\n");
    print_str("Total tokens: ");
    print_num((long)count);  // Cast to long to ensure proper handling
    print_str("\n");
    
    // Safety check
    if (count > MAX_TOKENS) {
        print_str("ERROR: Token count exceeds MAX_TOKENS!\n");
        return;
    }
    
    for (uint32_t i = 0; i < count; i++) {
        print_str("Token ");
        print_num(i);
        print_str(": type=");
        print_num(tokens[i].type);
        print_str(" (");
        print_str(token_type_name(tokens[i].type));
        print_str(") start=");
        print_num(tokens[i].start);
        print_str(" len=");
        print_num(tokens[i].len);
        print_str(" line=");
        print_num(tokens[i].line);
        print_str("\n");
    }
    print_str("=== END TOKEN DUMP ===\n");
}

// Fast byte lookup table for lexing
static const uint8_t char_types[256] = {
    [' '] = CHAR_WHITESPACE,
    ['\t'] = CHAR_WHITESPACE,
    ['\n'] = CHAR_WHITESPACE,
    ['\r'] = CHAR_WHITESPACE,
    
    ['|'] = CHAR_PIPE,
    ['/'] = CHAR_SLASH,
    ['\\'] = CHAR_BACKSLASH,
    ['<'] = CHAR_LT,
    ['>'] = CHAR_GT,
    ['^'] = CHAR_JUMP,
    ['!'] = CHAR_BANG,
    [':'] = CHAR_COLON,
    ['*'] = CHAR_STAR,
    ['-'] = CHAR_MINUS,
    ['['] = CHAR_LBRACKET,
    [']'] = CHAR_RBRACKET,
    
    ['0'...'9'] = CHAR_DIGIT,
    ['a'...'z'] = CHAR_ALPHA,
    ['A'...'Z'] = CHAR_ALPHA,
    ['_'] = CHAR_ALPHA,
    ['.'] = CHAR_DOT,
};

// Optimized string comparison - no strlen needed
static inline bool str_equals(const char* a, const char* b, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

// Keyword detection using perfect hash (compile-time computed)
static TokenType detect_keyword(const char* start, uint32_t len) {
    switch (len) {
        case 6:
            if (str_equals(start, "var.v-", 6)) return TOK_VAR;
            break;
        case 8:
            if (str_equals(start, "array.4d", 8)) return TOK_ARRAY_4D;
            if (str_equals(start, "fucn.can", 8)) return TOK_FUNC_CAN;
            break;
        case 11:
            if (str_equals(start, "error.catch", 11)) return TOK_ERROR_CATCH;
            if (str_equals(start, "gap.compute", 11)) return TOK_GAP_COMPUTE;
            break;
    }
    return TOK_IDENTIFIER;
}

// Main lexer - directly emits tokens to buffer
uint32_t lex_blaze(const char* input, uint32_t len, Token* output) {
    print_str("[LEXER] lex_blaze called UPDATED VERSION with len=");
    print_num((long)len);  // Explicit cast to long
    print_str(" input addr=");
    print_num((uint64_t)input);
    print_str(" output addr=");
    print_num((uint64_t)output);
    print_str(" CHECK_PRINT_WORKING\n");
    
    uint32_t pos = 0;
    uint32_t token_count = 0;
    
    while (pos < len) {
        // Skip whitespace using lookup table
        while (pos < len && char_types[(unsigned char)input[pos]] == CHAR_WHITESPACE) {
            pos++;
        }
        
        if (pos >= len) break;
        
        Token* tok = &output[token_count];
        tok->start = pos;
        
        
        uint8_t ch = input[pos];
        uint8_t ch_type = char_types[ch];
        
        if (ch == '*') {
            print_str("[LEX] Found * at pos=");
            print_num(pos);
            print_str(" next char='");
            if (pos + 1 < len) {
                char c = input[pos + 1];
                if (c >= 32 && c <= 126) {
                    char buf[2] = {c, 0};
                    print_str(buf);
                }
            }
            print_str("'\n");
        }
        
        // Multi-char tokens - hand-rolled state machine
        if (ch == '<') {
            if (pos + 2 < len && input[pos + 1] == '<' && input[pos + 2] == '.') {
                // Bitwise left shift: <<.
                tok->type = TOK_BIT_LSHIFT;
                tok->len = 3;
                pos += 3;
            } else if (pos + 1 < len) {
                if (input[pos + 1] == '<') {
                    tok->type = TOK_TIMING_ONTO;
                    tok->len = 2;
                    pos += 2;
                } else if (input[pos + 1] == '>') {
                    tok->type = TOK_TIMING_BOTH;
                    tok->len = 2;
                    pos += 2;
                } else {
                    tok->type = TOK_LT;
                    tok->len = 1;
                    pos++;
                }
            } else {
                tok->type = TOK_LT;
                tok->len = 1;
                pos++;
            }
        }
        else if (ch == '>') {
            if (pos + 2 < len && input[pos + 1] == '>' && input[pos + 2] == '.') {
                // Bitwise right shift: >>.
                tok->type = TOK_BIT_RSHIFT;
                tok->len = 3;
                pos += 3;
            } else if (pos + 1 < len && input[pos + 1] == '>') {
                tok->type = TOK_TIMING_INTO;
                tok->len = 2;
                pos += 2;
            } else {
                tok->type = TOK_GT;
                tok->len = 1;
                pos++;
            }
        }
        else if (ch == '\\') {
            if (pos + 2 < len) {
                if (input[pos + 1] == '>' && input[pos + 2] == '|') {
                    tok->type = TOK_CONNECTOR_FWD;
                    tok->len = 3;
                    pos += 3;
                } else if (input[pos + 1] == '<' && input[pos + 2] == '|') {
                    tok->type = TOK_CONNECTOR_BWD;
                    tok->len = 3;
                    pos += 3;
                } else {
                    tok->type = TOK_BACKSLASH;
                    tok->len = 1;
                    pos++;
                }
            } else {
                tok->type = TOK_BACKSLASH;
                tok->len = 1;
                pos++;
            }
        }
        else if (ch == 'd' && pos + 2 < len && input[pos + 1] == 'o' && input[pos + 2] == '/') {
            tok->type = TOK_ACTION_START;
            tok->len = 3;
            pos += 3;
        }
        else if (ch == ':' && pos + 1 < len && input[pos + 1] == '>') {
            tok->type = TOK_FUNC_CLOSE;
            tok->len = 2;
            pos += 2;
        }
        else if (ch == '*') {
            if (pos + 1 < len) {
                if (input[pos + 1] == '*') {
                    // Exponentiation operator
                    tok->type = TOK_EXPONENT;
                    tok->len = 2;
                    pos += 2;
                } else if (input[pos + 1] == '>') {
                    tok->type = TOK_GREATER_THAN;
                    tok->len = 2;
                    pos += 2;
                } else if (input[pos + 1] == '=') {
                    tok->type = TOK_EQUAL;
                    tok->len = 2;
                    pos += 2;
                } else if (pos + 2 < len && input[pos + 1] == '_' && input[pos + 2] == '<') {
                    tok->type = TOK_LESS_EQUAL;
                    tok->len = 3;
                    pos += 3;
                } else if (pos + 2 < len && input[pos + 1] == '!' && input[pos + 2] == '=') {
                    tok->type = TOK_NOT_EQUAL;
                    tok->len = 3;
                    pos += 3;
                } else {
                    tok->type = TOK_STAR;
                    tok->len = 1;
                    pos++;
                }
            } else {
                tok->type = TOK_STAR;
                tok->len = 1;
                pos++;
            }
        }
        else if (ch == '!' && pos + 1 < len && input[pos + 1] == '-') {
            tok->type = TOK_GLOBAL_ERROR;
            tok->len = 2;
            pos += 2;
            // Parse error number
            while (pos < len && char_types[(unsigned char)input[pos]] == CHAR_DIGIT) {
                tok->len++;
                pos++;
            }
        }
        else if (ch == '#' && pos + 1 < len && input[pos + 1] == '#') {
            // Skip comment
            pos += 2;
            while (pos < len && input[pos] != '\n') pos++;
            continue; // Don't emit token
        }
        else if (ch_type == CHAR_ALPHA) {
            // Check for print and other output keywords first
            if (pos + 5 <= len && str_equals(&input[pos], "print", 5)) {
                // Print keyword
                tok->type = TOK_PRINT;
                tok->len = 5;
                pos += 5;
            }
            else if (pos + 3 <= len && str_equals(&input[pos], "txt", 3)) {
                // Text output
                tok->type = TOK_TXT;
                tok->len = 3;
                pos += 3;
            }
            else if (pos + 3 <= len && str_equals(&input[pos], "out", 3)) {
                // Output
                tok->type = TOK_OUT;
                tok->len = 3;
                pos += 3;
            }
            else if (pos + 3 <= len && str_equals(&input[pos], "fmt", 3)) {
                // Format output
                tok->type = TOK_FMT;
                tok->len = 3;
                pos += 3;
            }
            else if (pos + 3 <= len && str_equals(&input[pos], "dyn", 3)) {
                // Dynamic output
                tok->type = TOK_DYN;
                tok->len = 3;
                pos += 3;
            }
            else if (pos + 3 <= len && str_equals(&input[pos], "asm", 3)) {
                // Assembly keyword
                tok->type = TOK_ASM;
                tok->len = 3;
                pos += 3;
            }
            else if (pos + 3 <= len && str_equals(&input[pos], "do/", 3)) {
                // Do action keyword
                tok->type = TOK_ACTION_START;
                tok->len = 3;
                pos += 3;
            }
            // Check for special keywords first
            // Check for generic var. pattern first (shortest match)
            else if (pos + 4 <= len && str_equals(&input[pos], "var.", 4)) {
                // Look ahead to determine which var pattern this is
                if (pos + 6 <= len && input[pos + 4] == 'v' && input[pos + 5] == '-') {
                    // This is var.v- pattern (old style)
                    tok->type = TOK_VAR;
                    uint32_t var_start = pos;
                    pos += 6; // Skip "var.v-"
                    
                    // Parse variable name
                    while (pos < len && (char_types[(unsigned char)input[pos]] == CHAR_ALPHA || 
                                        char_types[(unsigned char)input[pos]] == CHAR_DIGIT ||
                                        input[pos] == '_')) {
                        pos++;
                    }
                    
                    // Include trailing dash if present
                    if (pos < len && input[pos] == '-') {
                        pos++;
                    }
                    
                    tok->len = pos - var_start;
                }
                else if (pos + 6 <= len && input[pos + 4] == 'c' && input[pos + 5] == '-') {
                    // This is var.c- pattern (constant)
                    tok->type = TOK_CONST;
                    uint32_t const_start = pos;
                    pos += 6; // Skip "var.c-"
                    
                    // Parse constant name
                    while (pos < len && (char_types[(unsigned char)input[pos]] == CHAR_ALPHA || 
                                        char_types[(unsigned char)input[pos]] == CHAR_DIGIT ||
                                        input[pos] == '_')) {
                        pos++;
                    }
                    
                    // Include trailing dash if present
                    if (pos < len && input[pos] == '-') {
                        pos++;
                    }
                    
                    tok->len = pos - const_start;
                }
                else if (pos + 6 <= len && input[pos + 4] == 'i' && input[pos + 5] == '-') {
                    // This is var.i- pattern (integer)
                    tok->type = TOK_VAR_INT;
                    uint32_t var_start = pos;
                    pos += 6; // Skip "var.i-"
                    
                    // Parse variable name
                    while (pos < len && (char_types[(unsigned char)input[pos]] == CHAR_ALPHA || 
                                        char_types[(unsigned char)input[pos]] == CHAR_DIGIT ||
                                        input[pos] == '_')) {
                        pos++;
                    }
                    
                    // Include trailing dash if present
                    if (pos < len && input[pos] == '-') {
                        pos++;
                    }
                    
                    tok->len = pos - var_start;
                }
                else if (pos + 6 <= len && input[pos + 4] == 'f' && input[pos + 5] == '-') {
                    // This is var.f- pattern (float)
                    tok->type = TOK_VAR_FLOAT;
                    uint32_t var_start = pos;
                    pos += 6; // Skip "var.f-"
                    
                    // Parse variable name
                    while (pos < len && (char_types[(unsigned char)input[pos]] == CHAR_ALPHA || 
                                        char_types[(unsigned char)input[pos]] == CHAR_DIGIT ||
                                        input[pos] == '_')) {
                        pos++;
                    }
                    
                    // Include trailing dash if present
                    if (pos < len && input[pos] == '-') {
                        pos++;
                    }
                    
                    tok->len = pos - var_start;
                }
                else if (pos + 6 <= len && input[pos + 4] == 's' && input[pos + 5] == '-') {
                    // This is var.s- pattern (string)
                    tok->type = TOK_VAR_STRING;
                    uint32_t var_start = pos;
                    pos += 6; // Skip "var.s-"
                    
                    // Parse variable name
                    while (pos < len && (char_types[(unsigned char)input[pos]] == CHAR_ALPHA || 
                                        char_types[(unsigned char)input[pos]] == CHAR_DIGIT ||
                                        input[pos] == '_')) {
                        pos++;
                    }
                    
                    // Include trailing dash if present
                    if (pos < len && input[pos] == '-') {
                        pos++;
                    }
                    
                    tok->len = pos - var_start;
                }
                else if (pos + 6 <= len && input[pos + 4] == 'b' && input[pos + 5] == '-') {
                    // This is var.b- pattern (boolean)
                    tok->type = TOK_VAR_BOOL;
                    uint32_t var_start = pos;
                    pos += 6; // Skip "var.b-"
                    
                    // Parse variable name
                    while (pos < len && (char_types[(unsigned char)input[pos]] == CHAR_ALPHA || 
                                        char_types[(unsigned char)input[pos]] == CHAR_DIGIT ||
                                        input[pos] == '_')) {
                        pos++;
                    }
                    
                    // Include trailing dash if present
                    if (pos < len && input[pos] == '-') {
                        pos++;
                    }
                    
                    tok->len = pos - var_start;
                }
                else {
                    // This is var.name- pattern (simplified syntax)
                    uint32_t var_start = pos;
                    pos += 4; // Skip "var."
                    
                    // Parse variable name
                    uint32_t name_start = pos;
                    while (pos < len && (char_types[(unsigned char)input[pos]] == CHAR_ALPHA || 
                                        char_types[(unsigned char)input[pos]] == CHAR_DIGIT ||
                                        input[pos] == '_')) {
                        pos++;
                    }
                    
                    // Check for trailing dash (new simplified syntax)
                    if (pos < len && input[pos] == '-') {
                        // This is var.name- pattern, tokenize the whole thing
                        pos++; // Include the dash
                        tok->type = TOK_VAR;
                        tok->len = pos - var_start;
                    } else {
                        // Not our pattern, reset and treat as identifier
                        pos = var_start;
                        tok->type = TOK_IDENTIFIER;
                        tok->len = 3; // Just "var"
                        pos += 3;
                    }
                }
            }
            else if (pos + 8 <= len && str_equals(&input[pos], "array.4d", 8)) {
                // Array 4D keyword
                tok->type = TOK_ARRAY_4D;
                tok->len = 8;
                pos += 8;
            }
            else if (pos + 8 <= len && str_equals(&input[pos], "func.can", 8)) {
                // Function can keyword
                tok->type = TOK_FUNC_CAN;
                tok->len = 8;
                pos += 8;
            }
            else if (pos + 5 <= len && str_equals(&input[pos], "math.", 5)) {
                // Math function prefix
                tok->type = TOK_MATH_PREFIX;
                tok->len = 5;
                pos += 5;
            }
            else {
                // Regular identifier
                uint32_t start = pos;
                while (pos < len && (char_types[(unsigned char)input[pos]] == CHAR_ALPHA || 
                                    char_types[(unsigned char)input[pos]] == CHAR_DIGIT ||
                                    input[pos] == '_')) {
                    pos++;
                }
                tok->len = pos - start;
                tok->type = TOK_IDENTIFIER;
            }
        }
        else if (ch_type == CHAR_DIGIT || (ch == '0' && pos + 1 < len && (input[pos + 1] == 'x' || input[pos + 1] == 'X'))) {
            // Number (decimal, hex, float, long)
            tok->type = TOK_NUMBER;
            
            // Check for hex number
            if (ch == '0' && pos + 1 < len && (input[pos + 1] == 'x' || input[pos + 1] == 'X')) {
                pos += 2; // Skip 0x
                while (pos < len && ((input[pos] >= '0' && input[pos] <= '9') ||
                                   (input[pos] >= 'a' && input[pos] <= 'f') ||
                                   (input[pos] >= 'A' && input[pos] <= 'F'))) {
                    pos++;
                }
            } else {
                // Decimal integer part
                while (pos < len && char_types[(unsigned char)input[pos]] == CHAR_DIGIT) {
                    pos++;
                }
                
                // Fractional part
                if (pos < len && input[pos] == '.') {
                    pos++;
                    while (pos < len && char_types[(unsigned char)input[pos]] == CHAR_DIGIT) {
                        pos++;
                    }
                }
                
                // Scientific notation
                if (pos < len && (input[pos] == 'e' || input[pos] == 'E')) {
                    pos++;
                    if (pos < len && (input[pos] == '+' || input[pos] == '-')) {
                        pos++;
                    }
                    while (pos < len && char_types[(unsigned char)input[pos]] == CHAR_DIGIT) {
                        pos++;
                    }
                }
            }
            
            // Long suffix
            if (pos < len && (input[pos] == 'L' || input[pos] == 'l')) {
                pos++;
            }
            
            tok->len = pos - tok->start;
        }
        else if (ch == '"') {
            // String
            tok->type = TOK_STRING;
            pos++; // Skip opening quote
            uint32_t string_start = pos;
            while (pos < len && input[pos] != '"') {
                if (input[pos] == '\\' && pos + 1 < len) pos++; // Skip escaped char
                pos++;
            }
            if (pos < len) pos++; // Skip closing quote
            tok->len = pos - tok->start;
        }
        else {
            // Single character token
            if (ch == '<') {
                print_str("[LEXER] Entering single char switch with '<'\n");
            }
            switch (ch) {
                case '|': 
                    // Check for ||. (bitwise) or || (logical)
                    if (pos + 2 < len && input[pos + 1] == '|' && input[pos + 2] == '.') {
                        tok->type = TOK_BIT_OR;
                        tok->len = 3;
                        pos += 3;
                    } else if (pos + 1 < len && input[pos + 1] == '|') {
                        tok->type = TOK_OR;
                        tok->len = 2;
                        pos += 2;
                    } else {
                        tok->type = TOK_PIPE;
                        tok->len = 1;
                        pos++;
                    }
                    break;
                case '/': 
                    // Check if it's division or part of a Blaze operator
                    // Check if preceded by 'do/', 'v/', or output keywords (print/, txt/, out/, fmt/, dyn/, asm/)
                    if (pos > 0) {
                        bool is_slash = false;
                        
                        // Check for v/ (variable definition)
                        if (input[pos-1] == 'v') {
                            is_slash = true;
                        }
                        // Check for do/
                        else if (input[pos-1] == 'o' && pos >= 2 && input[pos-2] == 'd') {
                            is_slash = true;
                        }
                        // Check for print/
                        else if (pos >= 5 && str_equals(&input[pos-5], "print", 5)) {
                            is_slash = true;
                        }
                        // Check for txt/
                        else if (pos >= 3 && str_equals(&input[pos-3], "txt", 3)) {
                            is_slash = true;
                        }
                        // Check for out/
                        else if (pos >= 3 && str_equals(&input[pos-3], "out", 3)) {
                            is_slash = true;
                        }
                        // Check for fmt/
                        else if (pos >= 3 && str_equals(&input[pos-3], "fmt", 3)) {
                            is_slash = true;
                        }
                        // Check for dyn/
                        else if (pos >= 3 && str_equals(&input[pos-3], "dyn", 3)) {
                            is_slash = true;
                        }
                        // Check for asm/
                        else if (pos >= 3 && str_equals(&input[pos-3], "asm", 3)) {
                            is_slash = true;
                        }
                        
                        tok->type = is_slash ? TOK_SLASH : TOK_DIV;
                    } else {
                        tok->type = TOK_DIV; // Division operator
                    }
                    tok->len = 1;
                    pos++;
                    break;
                case '[': tok->type = TOK_BRACKET_OPEN; tok->len = 1; pos++; break;
                case ']': tok->type = TOK_BRACKET_CLOSE; tok->len = 1; pos++; break;
                case '^': 
                    // Check for ^^
                    if (pos + 1 < len && input[pos + 1] == '^') {
                        tok->type = TOK_BIT_XOR;
                        tok->len = 2;
                        pos += 2;
                    } else {
                        tok->type = TOK_JUMP_MARKER;
                        tok->len = 1;
                        pos++;
                    }
                    break;
                case '-': tok->type = TOK_MINUS; tok->len = 1; pos++; break;
                case '+': tok->type = TOK_PLUS; tok->len = 1; pos++; break;
                case '*': 
                    // Check for **
                    if (pos + 1 < len && input[pos + 1] == '*') {
                        print_str("[LEX] Found ** at pos=");
                        print_num(pos);
                        print_str("\n");
                        tok->type = TOK_EXPONENT;
                        tok->len = 2;
                        pos += 2;
                    } else {
                        tok->type = TOK_STAR;
                        tok->len = 1;
                        pos++;
                    }
                    break;
                case '%': tok->type = TOK_PERCENT; tok->len = 1; pos++; break;
                case ',': tok->type = TOK_COMMA; tok->len = 1; pos++; break;
                case '(': tok->type = TOK_LPAREN; tok->len = 1; pos++; break;
                case ')': tok->type = TOK_RPAREN; tok->len = 1; pos++; break;
                case '=': 
                    // Check for ==
                    if (pos + 1 < len && input[pos + 1] == '=') {
                        tok->type = TOK_EQ;
                        tok->len = 2;
                        pos += 2;
                    } else {
                        tok->type = TOK_EQUALS;
                        tok->len = 1;
                        pos++;
                    }
                    break;
                case '<':
                    // Check for <<., <=, or <<
                    print_str("[LEXER] Processing < at pos ");
                    print_num(pos);
                    print_str(" len=");
                    print_num(len);
                    if (pos + 2 < len) {
                        print_str(" next='");
                        char c = input[pos+1];
                        if (c >= 32 && c <= 126) {
                            char buf[2] = {c, 0};
                            print_str(buf);
                        } else {
                            print_num(c);
                        }
                        print_str("' next2='");
                        c = input[pos+2];
                        if (c >= 32 && c <= 126) {
                            char buf[2] = {c, 0};
                            print_str(buf);
                        } else {
                            print_num(c);
                        }
                        print_str("'");
                    }
                    print_str("\n");
                    if (pos + 2 < len && input[pos + 1] == '<' && input[pos + 2] == '.') {
                        // Bitwise left shift: <<.
                        print_str("[LEXER] Found <<.\n");
                        tok->type = TOK_BIT_LSHIFT;
                        tok->len = 3;
                        pos += 3;
                    } else if (pos + 1 < len && input[pos + 1] == '=') {
                        tok->type = TOK_LE;
                        tok->len = 2;
                        pos += 2;
                    } else if (pos + 1 < len && input[pos + 1] == '<') {
                        tok->type = TOK_TIMING_ONTO;
                        tok->len = 2;
                        pos += 2;
                    } else {
                        tok->type = TOK_LT_CMP;
                        tok->len = 1;
                        pos++;
                    }
                    break;
                case '>':
                    // Check for >>., >=, or >>
                    if (pos + 2 < len && input[pos + 1] == '>' && input[pos + 2] == '.') {
                        // Bitwise right shift: >>.
                        tok->type = TOK_BIT_RSHIFT;
                        tok->len = 3;
                        pos += 3;
                    } else if (pos + 1 < len && input[pos + 1] == '=') {
                        tok->type = TOK_GE;
                        tok->len = 2;
                        pos += 2;
                    } else if (pos + 1 < len && input[pos + 1] == '>') {
                        tok->type = TOK_TIMING_INTO;
                        tok->len = 2;
                        pos += 2;
                    } else {
                        tok->type = TOK_GT_CMP;
                        tok->len = 1;
                        pos++;
                    }
                    break;
                case '!':
                    // Check for !=
                    if (pos + 1 < len && input[pos + 1] == '=') {
                        tok->type = TOK_NE;
                        tok->len = 2;
                        pos += 2;
                    } else {
                        tok->type = TOK_BANG;
                        tok->len = 1;
                        pos++;
                    }
                    break;
                case '&':
                    // Check for &&. (bitwise) or && (logical)
                    if (pos + 2 < len && input[pos + 1] == '&' && input[pos + 2] == '.') {
                        tok->type = TOK_BIT_AND;
                        tok->len = 3;
                        pos += 3;
                    } else if (pos + 1 < len && input[pos + 1] == '&') {
                        tok->type = TOK_AND;
                        tok->len = 2;
                        pos += 2;
                    } else {
                        // Single & is an error in Blaze
                        tok->type = TOK_ERROR;
                        tok->len = 1;
                        pos++;
                    }
                    break;
                case '~': 
                    // Check for ~~
                    if (pos + 1 < len && input[pos + 1] == '~') {
                        tok->type = TOK_BIT_NOT;
                        tok->len = 2;
                        pos += 2;
                    } else {
                        // Single ~ is an error in Blaze
                        tok->type = TOK_ERROR;
                        tok->len = 1;
                        pos++;
                    }
                    break;
                default: 
                    tok->type = TOK_ERROR; 
                    tok->len = 1;
                    pos++;
                    break;
            }
        }
        
        token_count++;
        if (token_count >= MAX_TOKENS) {
            break;
        }
    }
    
    // Add EOF token
    if (token_count < MAX_TOKENS) {
        output[token_count].type = TOK_EOF;
        output[token_count].start = len;
        output[token_count].len = 0;
        token_count++;
    }
    
    print_str("[LEXER] lex_blaze returning token_count=");
    print_num(token_count);
    print_str("\n");
    
    return token_count;
}