// BLAZE LEXER - Updated for full Blaze syntax support
// Handles punctuation rules, parameters, matrices, etc.

#include "blaze_internals.h"

// Additional character types for Blaze-specific syntax
enum {
    CHAR_UNDERSCORE = 17,
    CHAR_AT = 18,
    CHAR_SEMICOLON = 19,
    CHAR_COMMA = 20,
    CHAR_PERCENT = 21,
    CHAR_EQUALS = 22,
    CHAR_LPAREN = 23,
    CHAR_RPAREN = 24,
    CHAR_LBRACE = 25,
    CHAR_RBRACE = 26
};

// Extended character lookup table
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
    ['.'] = CHAR_DOT,
    ['_'] = CHAR_UNDERSCORE,
    ['@'] = CHAR_AT,
    [';'] = CHAR_SEMICOLON,
    [','] = CHAR_COMMA,
    ['%'] = CHAR_PERCENT,
    ['='] = CHAR_EQUALS,
    ['('] = CHAR_LPAREN,
    [')'] = CHAR_RPAREN,
    ['{'] = CHAR_LBRACE,
    ['}'] = CHAR_RBRACE,
    
    ['0'...'9'] = CHAR_DIGIT,
    ['a'...'z'] = CHAR_ALPHA,
    ['A'...'Z'] = CHAR_ALPHA
};

// Token types are defined in blaze_internals.h

// String comparison helper
static inline bool str_equals(const char* a, const char* b, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

// Check if character sequence matches a pattern
static inline bool matches_pattern(const char* input, uint32_t pos, uint32_t max_len, const char* pattern, uint32_t pattern_len) {
    if (pos + pattern_len > max_len) return false;
    return str_equals(input + pos, pattern, pattern_len);
}

// Detect conditional abbreviations
static TokenType detect_conditional(const char* input, uint32_t pos, uint32_t max_len, uint32_t* consumed) {
    // Check for f. or fucn. prefix
    bool is_short = false;
    uint32_t prefix_len = 0;
    
    if (matches_pattern(input, pos, max_len, "f.", 2)) {
        is_short = true;
        prefix_len = 2;
    } else if (matches_pattern(input, pos, max_len, "fucn.", 5)) {
        is_short = false;
        prefix_len = 5;
    } else {
        return TOK_IDENTIFIER;
    }
    
    // Check abbreviation after prefix
    uint32_t abbr_pos = pos + prefix_len;
    
    // 3-letter abbreviations
    if (matches_pattern(input, abbr_pos, max_len, "ens", 3)) {
        *consumed = prefix_len + 3;
        return TOK_COND_ENS;
    } else if (matches_pattern(input, abbr_pos, max_len, "ver", 3)) {
        *consumed = prefix_len + 3;
        return TOK_COND_VER;
    } else if (matches_pattern(input, abbr_pos, max_len, "chk", 3)) {
        *consumed = prefix_len + 3;
        return TOK_COND_CHK;
    } else if (matches_pattern(input, abbr_pos, max_len, "try", 3)) {
        *consumed = prefix_len + 3;
        return TOK_COND_TRY;
    } else if (matches_pattern(input, abbr_pos, max_len, "grd", 3)) {
        *consumed = prefix_len + 3;
        return TOK_COND_GRD;
    } else if (matches_pattern(input, abbr_pos, max_len, "unl", 3)) {
        *consumed = prefix_len + 3;
        return TOK_COND_UNL;
    } else if (matches_pattern(input, abbr_pos, max_len, "whl", 3)) {
        *consumed = prefix_len + 3;
        return TOK_COND_WHL;
    } else if (matches_pattern(input, abbr_pos, max_len, "unt", 3)) {
        *consumed = prefix_len + 3;
        return TOK_COND_UNT;
    } else if (matches_pattern(input, abbr_pos, max_len, "obs", 3)) {
        *consumed = prefix_len + 3;
        return TOK_COND_OBS;
    } else if (matches_pattern(input, abbr_pos, max_len, "det", 3)) {
        *consumed = prefix_len + 3;
        return TOK_COND_DET;
    } else if (matches_pattern(input, abbr_pos, max_len, "rec", 3)) {
        *consumed = prefix_len + 3;
        return TOK_COND_REC;
    } else if (matches_pattern(input, abbr_pos, max_len, "rte", 3)) {
        *consumed = prefix_len + 3;
        return TOK_COND_RTE;
    } else if (matches_pattern(input, abbr_pos, max_len, "mon", 3)) {
        *consumed = prefix_len + 3;
        return TOK_COND_MON;
    } else if (matches_pattern(input, abbr_pos, max_len, "dec", 3)) {
        *consumed = prefix_len + 3;
        return TOK_COND_DEC;
    } else if (matches_pattern(input, abbr_pos, max_len, "ass", 3)) {
        *consumed = prefix_len + 3;
        return TOK_COND_ASS;
    } else if (matches_pattern(input, abbr_pos, max_len, "msr", 3)) {
        *consumed = prefix_len + 3;
        return TOK_COND_MSR;
    }
    
    // 2-letter abbreviations
    else if (matches_pattern(input, abbr_pos, max_len, "if", 2)) {
        *consumed = prefix_len + 2;
        return TOK_COND_IF;
    } else if (matches_pattern(input, abbr_pos, max_len, "fs", 2)) {
        *consumed = prefix_len + 2;
        return TOK_COND_FS;
    }
    
    // 4-letter abbreviations
    else if (matches_pattern(input, abbr_pos, max_len, "eval", 4)) {
        *consumed = prefix_len + 4;
        return TOK_COND_EVAL;
    }
    
    return TOK_IDENTIFIER;
}

// Skip comments
static uint32_t skip_comment(const char* input, uint32_t pos, uint32_t max_len) {
    if (pos + 2 <= max_len && input[pos] == '#' && input[pos + 1] == '#') {
        pos += 2;
        // Skip until next ## or newline
        while (pos < max_len) {
            if (input[pos] == '\n') {
                break;
            }
            if (pos + 2 <= max_len && input[pos] == '#' && input[pos + 1] == '#') {
                pos += 2;
                break;
            }
            pos++;
        }
    }
    return pos;
}

// Main lexer function - updated for Blaze syntax
uint32_t lex_blaze(const char* input, uint32_t len, Token* output) {
    uint32_t pos = 0;
    uint32_t token_count = 0;
    uint32_t line = 1;
    
    print_str("\n=== LEXER START ===\n");
    print_str("Input length: ");
    print_num(len);
    print_str("\nFirst 20 chars: '");
    for (uint32_t i = 0; i < len && i < 20; i++) {
        char c = input[i];
        if (c >= 32 && c <= 126) {
            char buf[2] = {c, '\0'};
            print_str(buf);
        } else if (c == '\n') {
            print_str("\\n");
        } else {
            print_str("?");
        }
    }
    print_str("'\n");
    
    while (pos < len && token_count < MAX_TOKENS - 1) {
        // Skip whitespace and track lines
        while (pos < len && char_types[input[pos]] == CHAR_WHITESPACE) {
            if (input[pos] == '\n') line++;
            pos++;
        }
        
        if (pos >= len) break;
        
        // Skip comments
        uint32_t new_pos = skip_comment(input, pos, len);
        if (new_pos != pos) {
            pos = new_pos;
            continue;
        }
        
        Token* tok = &output[token_count];
        tok->start = pos;
        tok->line = line;
        
        uint8_t ch = input[pos];
        uint8_t ch_type = char_types[ch];
        
        // Handle multi-character tokens
        
        // Parameter syntax: /{@param:
        if (ch == '/' && pos + 8 < len && matches_pattern(input, pos + 1, len, "{@param:", 8)) {
            tok->type = TOK_PARAM;
            tok->len = 9;
            pos += 9;
            token_count++;
            continue;
        }
        
        // Matrix start: [:::
        if (ch == '[' && pos + 3 < len && matches_pattern(input, pos, len, "[:::", 4)) {
            tok->type = TOK_MATRIX_START;
            tok->len = 4;
            pos += 4;
            token_count++;
            continue;
        }
        
        // Timeline definition: timeline-[
        if (matches_pattern(input, pos, len, "timeline-[", 10)) {
            tok->type = TOK_TIMELINE_DEF;
            tok->len = 10;
            pos += 10;
            token_count++;
            continue;
        }
        
        // Timeline jump: ^timeline.[
        if (ch == '^' && matches_pattern(input, pos + 1, len, "timeline.[", 10)) {
            tok->type = TOK_TIMELINE_JUMP;
            tok->len = 11;
            pos += 11;
            token_count++;
            continue;
        }
        
        // Variable definition: var.v-
        if (matches_pattern(input, pos, len, "var.v-", 6)) {
            tok->type = TOK_VAR;
            tok->len = 6;
            pos += 6;
            token_count++;
            continue;
        }
        
        // Array 4D: array.4d
        if (matches_pattern(input, pos, len, "array.4d", 8)) {
            tok->type = TOK_ARRAY_4D;
            tok->len = 8;
            pos += 8;
            token_count++;
            continue;
        }
        
        // GAP compute: gap.compute
        if (matches_pattern(input, pos, len, "gap.compute", 11)) {
            tok->type = TOK_GAP_COMPUTE;
            tok->len = 11;
            pos += 11;
            token_count++;
            continue;
        }
        
        
        // Split operations
        if (matches_pattern(input, pos, len, "c.split._", 9)) {
            tok->type = TOK_C_SPLIT;
            tok->len = 9;
            pos += 9;
            token_count++;
            continue;
        }
        
        // Action start: do/
        if (matches_pattern(input, pos, len, "do/", 3)) {
            tok->type = TOK_ACTION_START;
            tok->len = 3;
            pos += 3;
            token_count++;
            continue;
        }
        
        // Comparison operators
        if (ch == '*') {
            if (pos + 1 < len) {
                if (input[pos + 1] == '>') {
                    tok->type = TOK_GREATER_THAN;
                    tok->len = 2;
                    pos += 2;
                    token_count++;
                    continue;
                } else if (input[pos + 1] == '=') {
                    tok->type = TOK_EQUAL;
                    tok->len = 2;
                    pos += 2;
                    token_count++;
                    continue;
                } else if (pos + 2 < len && input[pos + 1] == '_' && input[pos + 2] == '<') {
                    tok->type = TOK_LESS_EQUAL;
                    tok->len = 3;
                    pos += 3;
                    token_count++;
                    continue;
                } else if (pos + 2 < len && input[pos + 1] == '!' && input[pos + 2] == '=') {
                    tok->type = TOK_NOT_EQUAL;
                    tok->len = 3;
                    pos += 3;
                    token_count++;
                    continue;
                }
            }
            tok->type = TOK_STAR;
            tok->len = 1;
            pos++;
            token_count++;
            continue;
        }
        
        // Temporal operators
        if (ch == '<') {
            if (pos + 1 < len && input[pos + 1] == '<') {
                tok->type = TOK_TIMING_ONTO;
                tok->len = 2;
                pos += 2;
            } else if (pos + 1 < len && input[pos + 1] == '>') {
                tok->type = TOK_TIMING_BOTH;
                tok->len = 2;
                pos += 2;
            } else {
                tok->type = TOK_LT;
                tok->len = 1;
                pos++;
            }
            token_count++;
            continue;
        }
        
        if (ch == '>') {
            if (pos + 1 < len && input[pos + 1] == '>') {
                tok->type = TOK_TIMING_INTO;
                tok->len = 2;
                pos += 2;
            } else {
                tok->type = TOK_GT;
                tok->len = 1;
                pos++;
            }
            token_count++;
            continue;
        }
        
        // Connectors
        if (ch == '\\') {
            if (pos + 2 < len && input[pos + 1] == '>' && input[pos + 2] == '|') {
                tok->type = TOK_CONNECTOR_FWD;
                tok->len = 3;
                pos += 3;
            } else if (pos + 2 < len && input[pos + 1] == '<' && input[pos + 2] == '|') {
                tok->type = TOK_CONNECTOR_BWD;
                tok->len = 3;
                pos += 3;
            } else {
                tok->type = TOK_BACKSLASH;
                tok->len = 1;
                pos++;
            }
            token_count++;
            continue;
        }
        
        // Conditionals (f. or fucn.)
        if (ch == 'f' && (matches_pattern(input, pos, len, "f.", 2) || 
                          matches_pattern(input, pos, len, "fucn.", 5))) {
            uint32_t consumed = 0;
            TokenType cond_type = detect_conditional(input, pos, len, &consumed);
            if (cond_type != TOK_IDENTIFIER) {
                tok->type = cond_type;
                tok->len = consumed;
                pos += consumed;
                token_count++;
                continue;
            }
        }
        
        // Keywords and identifiers
        if (ch_type == CHAR_ALPHA) {
            print_str("Found alpha at pos ");
            print_num(pos);
            print_str(": '");
            char c[2] = {ch, '\0'};
            print_str(c);
            print_str("'\n");
            
            uint32_t start = pos;
            while (pos < len && (char_types[input[pos]] == CHAR_ALPHA || 
                                char_types[input[pos]] == CHAR_DIGIT ||
                                input[pos] == '.' || input[pos] == '-' || input[pos] == '_')) {
                pos++;
            }
            
            uint32_t word_len = pos - start;
            
            // Check for keywords
            print_str("Checking keyword: '");
            for (uint32_t j = 0; j < word_len && j < 20; j++) {
                char c[2] = {input[start + j], '\0'};
                print_str(c);
            }
            print_str("' len=");
            print_num(word_len);
            print_str("\n");
            
            if (word_len == 7) {
                print_str("Word of length 7 at pos ");
                print_num(start);
                print_str(": bytes = ");
                for (uint32_t j = 0; j < 7; j++) {
                    print_num((uint8_t)input[start + j]);
                    print_str(" ");
                }
                print_str("\n");
                print_str("Expected: 100 101 99 108 97 114 101 (declare)\n");
                if (str_equals(input + start, "declare", 7)) {
                    print_str("MATCHED DECLARE!\n");
                    tok->type = TOK_DECLARE;
                } else {
                    print_str("NO MATCH\n");
                    tok->type = TOK_IDENTIFIER;
                }
            } else {
                tok->type = TOK_IDENTIFIER;
            }
            if (tok->type == 0) {  // Only check other keywords if not already set
                if (str_equals(input + start, "bnc", 3) && word_len == 3) {
                    tok->type = TOK_BNC;
                } else if (str_equals(input + start, "recv", 4) && word_len == 4) {
                    tok->type = TOK_RECV;
                } else if (str_equals(input + start, "past_zone", 9) && word_len == 9) {
                    tok->type = TOK_PAST_ZONE;
                } else if (str_equals(input + start, "present_zone", 12) && word_len == 12) {
                    tok->type = TOK_PRESENT_ZONE;
                } else if (str_equals(input + start, "future_zone", 11) && word_len == 11) {
                    tok->type = TOK_FUTURE_ZONE;
                } else if (str_equals(input + start, "unknown_zone", 12) && word_len == 12) {
                    tok->type = TOK_UNKNOWN_ZONE;
                } else if (tok->type == 0) {
                    tok->type = TOK_IDENTIFIER;
                }
            }
            
            tok->len = word_len;
            token_count++;
            continue;
        }
        
        // Numbers
        if (ch_type == CHAR_DIGIT) {
            uint32_t start = pos;
            while (pos < len && char_types[input[pos]] == CHAR_DIGIT) {
                pos++;
            }
            // Check for decimal point
            if (pos < len && input[pos] == '.' && pos + 1 < len && 
                char_types[input[pos + 1]] == CHAR_DIGIT) {
                pos++; // Skip dot
                while (pos < len && char_types[input[pos]] == CHAR_DIGIT) {
                    pos++;
                }
            }
            tok->type = TOK_NUMBER;
            tok->len = pos - start;
            token_count++;
            continue;
        }
        
        // Single character tokens
        switch (ch) {
            case '|': tok->type = TOK_PIPE; break;
            case '/': tok->type = TOK_SLASH; break;
            case '^': tok->type = TOK_JUMP_MARKER; break;
            case '!': tok->type = TOK_BANG; break;
            case ':': tok->type = TOK_COLON; break;
            case '-': tok->type = TOK_MINUS; break;
            case '[': tok->type = TOK_BRACKET_OPEN; break;
            case ']': tok->type = TOK_BRACKET_CLOSE; break;
            case '.': tok->type = TOK_DOT; break;
            case '_': tok->type = TOK_UNDERSCORE; break;
            case '@': tok->type = TOK_AT; break;
            case ';': tok->type = TOK_SEMICOLON; break;
            case ',': tok->type = TOK_COMMA; break;
            case '%': tok->type = TOK_PERCENT; break;
            case '=': tok->type = TOK_EQUALS; break;
            case '(': tok->type = TOK_LPAREN; break;
            case ')': tok->type = TOK_RPAREN; break;
            case '{': tok->type = TOK_LBRACE; break;
            case '}': tok->type = TOK_RBRACE; break;
            default:
                tok->type = TOK_ERROR;
        }
        
        tok->len = 1;
        pos++;
        token_count++;
    }
    
    // Add EOF token
    if (token_count < MAX_TOKENS) {
        output[token_count].type = TOK_EOF;
        output[token_count].start = pos;
        output[token_count].len = 0;
        output[token_count].line = line;
        token_count++;
    }
    
    return token_count;
}

// Debug token printer
void debug_print_tokens(Token* tokens, uint32_t count, const char* source) {
    print_str("\n=== TOKENS ===\n");
    for (uint32_t i = 0; i < count; i++) {
        Token* t = &tokens[i];
        print_str("Line ");
        print_num(t->line);
        print_str(": ");
        
        // Print token type
        switch (t->type) {
            case TOK_VAR: print_str("VAR"); break;
            case TOK_PARAM: print_str("PARAM"); break;
            case TOK_MATRIX_START: print_str("MATRIX"); break;
            case TOK_TIMELINE_DEF: print_str("TIMELINE_DEF"); break;
            case TOK_TIMELINE_JUMP: print_str("TIMELINE_JUMP"); break;
            case TOK_ACTION_START: print_str("ACTION_START"); break;
            case TOK_COND_CHK: print_str("COND_CHK"); break;
            case TOK_COND_ENS: print_str("COND_ENS"); break;
            case TOK_COND_VER: print_str("COND_VER"); break;
            case TOK_IDENTIFIER: print_str("IDENT"); break;
            case TOK_NUMBER: print_str("NUMBER"); break;
            case TOK_EOF: print_str("EOF"); break;
            default: print_str("OTHER"); break;
        }
        
        print_str(" '");
        // Print actual token text
        for (uint32_t j = 0; j < t->len && j < 20; j++) {
            uint8_t c = (uint8_t)source[t->start + j];
            print_num(c);
            print_str(" ");
        }
        print_str("' (type=");
        print_num(t->type);
        print_str(")\n");
    }
    print_str("=== END TOKENS ===\n");
}