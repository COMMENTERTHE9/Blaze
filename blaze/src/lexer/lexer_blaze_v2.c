// BLAZE LEXER V2 - Properly handles Blaze syntax patterns
// Correctly parses var.v-name-[value] and other Blaze constructs

#include "blaze_internals.h"

// Character classification for Blaze
static inline bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static inline bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static inline bool is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

// Check if character can be part of identifier
static inline bool is_ident_char(char c) {
    return is_alnum(c) || c == '_' || c == '-' || c == '.';
}

// Skip whitespace and update line number
static uint32_t skip_whitespace(const char* input, uint32_t pos, uint32_t len, uint32_t* line) {
    while (pos < len && is_whitespace(input[pos])) {
        if (input[pos] == '\n') (*line)++;
        pos++;
    }
    return pos;
}

// Skip comment
static uint32_t skip_comment(const char* input, uint32_t pos, uint32_t len) {
    if (pos + 1 < len) {
        // Blaze block comment style: ## comment ##
        if (input[pos] == '#' && input[pos + 1] == '#') {
            pos += 2;
            // Skip until end of line or next ##
            while (pos < len && input[pos] != '\n') {
                if (pos + 1 < len && input[pos] == '#' && input[pos + 1] == '#') {
                    pos += 2;
                    break;
                }
                pos++;
            }
        /* Blaze comment syntax is only ## comment ## (double-hash). */
        }
    }
    return pos;
}

// Match a string pattern
static bool match_string(const char* input, uint32_t pos, uint32_t len, const char* pattern) {
    uint32_t plen = 0;
    while (pattern[plen]) plen++;
    
    if (pos + plen > len) return false;
    
    for (uint32_t i = 0; i < plen; i++) {
        if (input[pos + i] != pattern[i]) return false;
    }
    return true;
}

// Parse variable declaration: var.i-name-[value], var.f-name-[value], etc.
static uint32_t parse_var_decl(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    TokenType var_type = TOK_VAR;
    uint32_t skip_len = 0;
    
    if (match_string(input, pos, len, "var.v-")) {
        var_type = TOK_VAR;
        skip_len = 6;
    } else if (match_string(input, pos, len, "var.i-")) {
        var_type = TOK_VAR_INT;
        skip_len = 6;
    } else if (match_string(input, pos, len, "var.f-")) {
        var_type = TOK_VAR_FLOAT;
        skip_len = 6;
    } else if (match_string(input, pos, len, "var.s-")) {
        var_type = TOK_VAR_STRING;
        skip_len = 6;
    } else if (match_string(input, pos, len, "var.b-")) {
        var_type = TOK_VAR_BOOL;
        skip_len = 6;
    } else if (match_string(input, pos, len, "var.c-")) {
        var_type = TOK_CONST;
        skip_len = 6;
    } else if (match_string(input, pos, len, "var.d-")) {
        var_type = TOK_VAR_SOLID;
        skip_len = 6;
    } else {
        return 0;
    }
    
    uint32_t start = pos;
    pos += skip_len;
    
    // Now we need to find the variable name
    uint32_t name_start = pos;
    
    // Variable name can contain letters, digits, underscores
    while (pos < len && (is_alnum(input[pos]) || input[pos] == '_')) {
        pos++;
    }
    
    // Check if there's an initialization
    if (pos < len && input[pos] == '-' && pos + 1 < len && input[pos + 1] == '[') {
        // This is var.i-name-[value], var.f-name-[value], etc. pattern
        pos++; // Skip '-'
        tok->type = var_type;
        tok->len = pos - start;
        return pos;
    } else if (pos > name_start) {
        // Just var.i-name, var.f-name, etc.
        tok->type = var_type;
        tok->len = pos - start;
        return pos;
    }
    
    return 0;
}

// Parse identifier
static uint32_t parse_identifier(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    if (!is_alpha(input[pos]) && input[pos] != '_') return 0;
    
    uint32_t start = pos;
    while (pos < len && (is_alnum(input[pos]) || input[pos] == '_')) {
        pos++;
    }
    
    tok->type = TOK_IDENTIFIER;
    tok->len = pos - start;
    
    // Check for keywords
    uint32_t word_len = pos - start;
    if (word_len == 7 && match_string(input, start, len, "declare")) {
        tok->type = TOK_DECLARE;
    } else if (word_len == 3 && match_string(input, start, len, "bnc")) {
        tok->type = TOK_BNC;
    } else if (word_len == 4 && match_string(input, start, len, "recv")) {
        tok->type = TOK_RECV;
    } else if (word_len == 2 && match_string(input, start, len, "if")) {
        // Direct if keyword
        tok->type = TOK_COND_IF;
    } else if (word_len == 5 && match_string(input, start, len, "while")) {
        // Direct while keyword  
        tok->type = TOK_COND_WHL;
    } else if (word_len == 4 && match_string(input, start, len, "else")) {
        // Direct else keyword
        tok->type = TOK_ELSE;
    } else if (word_len == 4 && match_string(input, start, len, "func")) {
        // Check if this is followed by .can
        if (pos + 4 < len && input[pos] == '.' && match_string(input, pos + 1, len - (pos + 1), "can")) {
            tok->type = TOK_FUNC_CAN;
            tok->len = pos + 4 - start; // Include the .can part
            return pos + 4;
        }
    } else if (word_len == 4 && match_string(input, start, len, "verb")) {
        // Check if this is followed by .can
        if (pos + 4 < len && input[pos] == '.' && match_string(input, pos + 1, len - (pos + 1), "can")) {
            tok->type = TOK_FUNC_CAN;
            tok->len = pos + 4 - start; // Include the .can part
            return pos + 4;
        }
    } else if (word_len == 4 && match_string(input, start, len, "gggx")) {
        // Check for GGGX commands
        if (pos + 5 < len && input[pos] == '.' && match_string(input, pos + 1, len - (pos + 1), "init")) {
            tok->type = TOK_GGGX_INIT;
            tok->len = pos + 5 - start; // Include the .init part
            return pos + 5;
        } else if (pos + 3 < len && input[pos] == '.' && match_string(input, pos + 1, len - (pos + 1), "go")) {
            tok->type = TOK_GGGX_GO;
            tok->len = pos + 3 - start; // Include the .go part
            return pos + 3;
        } else if (pos + 4 < len && input[pos] == '.' && match_string(input, pos + 1, len - (pos + 1), "get")) {
            tok->type = TOK_GGGX_GET;
            tok->len = pos + 4 - start; // Include the .get part
            return pos + 4;
        } else if (pos + 4 < len && input[pos] == '.' && match_string(input, pos + 1, len - (pos + 1), "gap")) {
            tok->type = TOK_GGGX_GAP;
            tok->len = pos + 4 - start; // Include the .gap part
            return pos + 4;
        } else if (pos + 8 < len && input[pos] == '.' && match_string(input, pos + 1, len - (pos + 1), "glimpse")) {
            tok->type = TOK_GGGX_GLIMPSE;
            tok->len = pos + 8 - start; // Include the .glimpse part
            return pos + 8;
        } else if (pos + 6 < len && input[pos] == '.' && match_string(input, pos + 1, len - (pos + 1), "guess")) {
            tok->type = TOK_GGGX_GUESS;
            tok->len = pos + 6 - start; // Include the .guess part
            return pos + 6;
        } else if (pos + 4 < len && input[pos] == '.' && match_string(input, pos + 1, len - (pos + 1), "set")) {
            tok->type = TOK_GGGX_SET;
            tok->len = pos + 4 - start; // Include the .set part
            return pos + 4;
        } else if (pos + 7 < len && input[pos] == '.' && match_string(input, pos + 1, len - (pos + 1), "enable")) {
            tok->type = TOK_GGGX_ENABLE;
            tok->len = pos + 7 - start; // Include the .enable part
            return pos + 7;
        } else if (pos + 7 < len && input[pos] == '.' && match_string(input, pos + 1, len - (pos + 1), "status")) {
            tok->type = TOK_GGGX_STATUS;
            tok->len = pos + 7 - start; // Include the .status part
            return pos + 7;
        } else if (pos + 6 < len && input[pos] == '.' && match_string(input, pos + 1, len - (pos + 1), "print")) {
            tok->type = TOK_GGGX_PRINT;
            tok->len = pos + 6 - start; // Include the .print part
            return pos + 6;
        } else if (pos + 8 < len && input[pos] == '.' && match_string(input, pos + 1, len - (pos + 1), "analyze")) {
            tok->type = TOK_GGGX_ANALYZE;
            tok->len = pos + 8 - start; // Include the .analyze part
            return pos + 8;
        }
    }
    
    return pos;
}

// Parse number
static uint32_t parse_number(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    if (!is_digit(input[pos])) return 0;
    
    uint32_t start = pos;
    while (pos < len && is_digit(input[pos])) {
        pos++;
    }
    
    // Check for decimal
    if (pos + 1 < len && input[pos] == '.' && is_digit(input[pos + 1])) {
        pos++; // Skip '.'
        while (pos < len && is_digit(input[pos])) {
            pos++;
        }
    }
    
    tok->type = TOK_NUMBER;
    tok->len = pos - start;
    return pos;
}

// Parse function definition: |name|
static uint32_t parse_function_def(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    if (input[pos] != '|') return 0;
    
    uint32_t start = pos;
    pos++; // Skip first |
    
    // Find closing |
    while (pos < len && input[pos] != '|') {
        pos++;
    }
    
    if (pos < len && input[pos] == '|') {
        pos++; // Include closing |
        tok->type = TOK_PIPE; // For now, treat as pipe pairs
        tok->len = pos - start;
        return pos;
    }
    
    return 0;
}

// Parse parameter after < has been consumed: {@param:name}
static uint32_t parse_parameter_after_lt(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    uint32_t start = pos;
    print_str("[LEXER] parse_parameter_after_lt called at pos "); print_num(pos); print_str("\n");
    print_str("[LEXER] parse_parameter_after_lt: next 10 chars: '");
    for (int i = 0; i < 10 && pos + i < len; i++) {
        char c = input[pos + i];
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
    // Skip optional whitespace
    while (pos < len && (input[pos] == ' ' || input[pos] == '\t')) pos++;
    if (pos + 7 < len && input[pos] == '{' && match_string(input, pos + 1, len - (pos + 1), "@param:")) {
        print_str("[LEXER] parse_parameter_after_lt: found {@param: at pos "); print_num(pos); print_str("\n");
        pos += 1 + 7; // Skip '{@param:'
        // Find the closing }
        while (pos < len && input[pos] != '}') {
            if (!is_alnum(input[pos]) && input[pos] != '_') {
                print_str("[LEXER] parse_parameter_after_lt: invalid char in param name\n");
                return 0; // Invalid character in parameter name
            }
            pos++;
        }
        if (pos < len && input[pos] == '}') {
            pos++; // Include the closing }
            tok->type = TOK_PARAM;
            tok->start = start;
            tok->len = pos - start;
            print_str("[LEXER] parse_parameter_after_lt: success, len="); print_num(tok->len); print_str("\n");
            return pos;
        }
    }
    print_str("[LEXER] parse_parameter_after_lt: no match\n");
    return 0;
}

// Parse parameter: < {@param:name} or /{@param:name}
static uint32_t parse_parameter(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    // Accept either < {@param:name} or /{@param:name}
    uint32_t start = pos;
    // Skip optional whitespace
    while (pos < len && (input[pos] == ' ' || input[pos] == '\t')) pos++;
    if (pos < len && (input[pos] == '<' || input[pos] == '/')) {
        pos++;
        // Skip optional whitespace
        while (pos < len && (input[pos] == ' ' || input[pos] == '\t')) pos++;
        if (pos + 7 < len && input[pos] == '{' && match_string(input, pos + 1, len - (pos + 1), "@param:")) {
            pos += 1 + 7; // Skip '{@param:'
            // Find the closing }
            while (pos < len && input[pos] != '}') {
                if (!is_alnum(input[pos]) && input[pos] != '_') {
                    return 0; // Invalid character in parameter name
                }
                pos++;
            }
            if (pos < len && input[pos] == '}') {
                pos++; // Include the closing }
                tok->type = TOK_PARAM;
                tok->len = pos - start;
                return pos;
            }
        }
    }
    return 0;
}

// Parse timeline: timeline-[ or ^timeline.[
static uint32_t parse_timeline(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    if (match_string(input, pos, len, "timeline-[")) {
        tok->type = TOK_TIMELINE_DEF;
        tok->len = 10;
        return pos + 10;
    }
    
    if (input[pos] == '^' && match_string(input, pos + 1, len - 1, "timeline.[")) {
        tok->type = TOK_TIMELINE_JUMP;
        tok->len = 11;
        return pos + 11;
    }
    
    return 0;
}

// Parse fixed points: fix.p-[ or f.p-[ or f.p (inline)
static uint32_t parse_fixed_point(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    if (match_string(input, pos, len, "fix.p-[")) {
        tok->type = TOK_FIX_P;
        tok->len = 7;
        return pos + 7;
    }
    
    if (match_string(input, pos, len, "f.p-[")) {
        tok->type = TOK_F_P;
        tok->len = 5;
        return pos + 5;
    }
    
    // Also check for inline f.p without bracket
    if (match_string(input, pos, len, "f.p") && 
        (pos + 3 >= len || (!is_alnum(input[pos + 3]) && input[pos + 3] != '-'))) {
        tok->type = TOK_F_P;
        tok->len = 3;
        return pos + 3;
    }
    
    return 0;
}

// Parse permanent timelines: timelineper-[ or timelinep-[ or ^timelinep.[
static uint32_t parse_permanent_timeline(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    if (match_string(input, pos, len, "timelineper-[")) {
        tok->type = TOK_TIMELINE_PER;
        tok->len = 13;
        return pos + 13;
    }
    
    if (match_string(input, pos, len, "timelinep-[")) {
        tok->type = TOK_TIMELINE_P;
        tok->len = 11;
        return pos + 11;
    }
    
    if (input[pos] == '^' && match_string(input, pos + 1, len - 1, "timelinep.[")) {
        tok->type = TOK_TIMELINE_P_JUMP;
        tok->len = 12;
        return pos + 12;
    }
    
    return 0;
}

// Parse action block: do/
static uint32_t parse_action(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    print_str("[LEXER] parse_action called at pos "); print_num(pos); print_str("\n");
    
    if (pos + 2 >= len) {
        print_str("[LEXER] parse_action: not enough chars\n");
        return 0;
    }
    
    print_str("[LEXER] parse_action: checking '"); 
    for (int i = 0; i < 3 && pos + i < len; i++) {
        char buf[2] = {input[pos + i], '\0'};
        print_str(buf);
    }
    print_str("'\n");
    
    if (input[pos] == 'd' && input[pos + 1] == 'o' && input[pos + 2] == '/') {
        print_str("[LEXER] parse_action: found 'do/' at pos "); print_num(pos); print_str("\n");
        tok->type = TOK_ACTION_START;
        tok->len = 3;
        return pos + 3;
    }
    
    print_str("[LEXER] parse_action: no match\n");
    return 0;
}

// Parse block end marker: :>
static uint32_t parse_block_end(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    if (pos + 1 < len && input[pos] == ':' && input[pos + 1] == '>') {
        tok->type = TOK_BLOCK_END;
        tok->len = 2;
        return pos + 2;
    }
    return 0;
}

// Parse time-bridge operators: >/>, >\>, </<, <\<
static uint32_t parse_time_bridge(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    if (pos + 2 < len) {
        if (input[pos] == '>' && input[pos + 1] == '/' && input[pos + 2] == '>') {
            tok->type = TOK_TIME_BRIDGE_FWD;
            tok->len = 3;
            return pos + 3;
        }
        if (input[pos] == '>' && input[pos + 1] == '\\' && input[pos + 2] == '>') {
            tok->type = TOK_SLOW_FWD;
            tok->len = 3;
            return pos + 3;
        }
        if (input[pos] == '<' && input[pos + 1] == '/' && input[pos + 2] == '<') {
            tok->type = TOK_FAST_REWIND;
            tok->len = 3;
            return pos + 3;
        }
        if (input[pos] == '<' && input[pos + 1] == '\\' && input[pos + 2] == '<') {
            tok->type = TOK_SLOW_REWIND;
            tok->len = 3;
            return pos + 3;
        }
    }
    return 0;
}

// Parse array: array.4d
static uint32_t parse_array(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    if (match_string(input, pos, len, "array.4d")) {
        tok->type = TOK_ARRAY_4D;
        tok->len = 8;
        return pos + 8;
    }
    return 0;
}

// Parse GAP: gap.compute
static uint32_t parse_gap(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    if (match_string(input, pos, len, "gap.compute")) {
        tok->type = TOK_GAP_COMPUTE;
        tok->len = 11;
        return pos + 11;
    }
    return 0;
}

// Parse connectors: \>| or \<|
static uint32_t parse_connector(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    if (pos + 2 < len && input[pos] == '\\') {
        if (input[pos + 1] == '>' && input[pos + 2] == '|') {
            tok->type = TOK_CONNECTOR_FWD;
            tok->len = 3;
            return pos + 3;
        }
        if (input[pos + 1] == '<' && input[pos + 2] == '|') {
            tok->type = TOK_CONNECTOR_BWD;
            tok->len = 3;
            return pos + 3;
        }
    }
    return 0;
}

// Parse temporal operators
static uint32_t parse_temporal_op(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    if (input[pos] == '<') {
        if (pos + 1 < len && input[pos + 1] == '<') {
            tok->type = TOK_TIMING_ONTO;
            tok->len = 2;
            return pos + 2;
        } else if (pos + 1 < len && input[pos + 1] == '>') {
            tok->type = TOK_TIMING_BOTH;
            tok->len = 2;
            return pos + 2;
        } else {
            tok->type = TOK_LT;
            tok->len = 1;
            return pos + 1;
        }
    }
    
    if (input[pos] == '>') {
        if (pos + 1 < len && input[pos + 1] == '>') {
            tok->type = TOK_TIMING_INTO;
            tok->len = 2;
            return pos + 2;
        } else {
            tok->type = TOK_GT;
            tok->len = 1;
            return pos + 1;
        }
    }
    
    return 0;
}

// Parse output methods: print/, txt/, out/, fmt/, dyn/
static uint32_t parse_output_method(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    print_str("[LEXER] parse_output_method called at pos "); print_num(pos); print_str("\n");
    if (match_string(input, pos, len, "print/")) {
        print_str("[LEXER] Found print/ at pos "); print_num(pos); print_str("\n");
        tok->type = TOK_PRINT;
        tok->len = 6;
        return pos + 6;
    }
    
    if (match_string(input, pos, len, "txt/")) {
        tok->type = TOK_TXT;
        tok->len = 4;
        return pos + 4;
    }
    
    if (match_string(input, pos, len, "out/")) {
        tok->type = TOK_OUT;
        tok->len = 4;
        return pos + 4;
    }
    
    if (match_string(input, pos, len, "fmt/")) {
        tok->type = TOK_FMT;
        tok->len = 4;
        return pos + 4;
    }
    
    if (match_string(input, pos, len, "dyn/")) {
        tok->type = TOK_DYN;
        tok->len = 4;
        return pos + 4;
    }
    
    if (match_string(input, pos, len, "asm/")) {
        tok->type = TOK_ASM;
        tok->len = 4;
        return pos + 4;
    }
    
    // Detect return/
    if (pos + 6 < len && strncmp(&input[pos], "return/", 7) == 0) {
        tok->type = TOK_RETURN;
        tok->len = 7;
        return pos + 7;
    }
    
    return 0;
}

// Parse c.split._, cac._, or Crack._ 
static uint32_t parse_split(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    // Check for c.split._
    if (match_string(input, pos, len, "c.split._")) {
        tok->type = TOK_C_SPLIT;
        tok->start = pos;
        
        // Find the bracketed content
        uint32_t end = pos + 9; // Length of "c.split._"
        if (end < len && input[end] == '[') {
            end++;
            int bracket_depth = 1;
            while (end < len && bracket_depth > 0) {
                if (input[end] == '[') bracket_depth++;
                else if (input[end] == ']') bracket_depth--;
                end++;
            }
        }
        
        tok->len = end - pos;
        return end;
    }
    
    // Check for cac._
    if (match_string(input, pos, len, "cac._")) {
        tok->type = TOK_C_SPLIT;
        tok->start = pos;
        
        uint32_t end = pos + 5; // Length of "cac._"
        if (end < len && input[end] == '[') {
            end++;
            int bracket_depth = 1;
            while (end < len && bracket_depth > 0) {
                if (input[end] == '[') bracket_depth++;
                else if (input[end] == ']') bracket_depth--;
                end++;
            }
        }
        
        tok->len = end - pos;
        return end;
    }
    
    // Check for Crack._
    if (match_string(input, pos, len, "Crack._")) {
        tok->type = TOK_C_SPLIT;
        tok->start = pos;
        
        uint32_t end = pos + 7; // Length of "Crack._"
        if (end < len && input[end] == '[') {
            end++;
            int bracket_depth = 1;
            while (end < len && bracket_depth > 0) {
                if (input[end] == '[') bracket_depth++;
                else if (input[end] == ']') bracket_depth--;
                end++;
            }
        }
        
        tok->len = end - pos;
        return end;
    }
    
    return 0;
}

// Parse matrix: [:::name1-name2-name3[values]]
static uint32_t parse_matrix(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    // Check for [:::
    if (pos + 4 < len && input[pos] == '[' && 
        input[pos + 1] == ':' && input[pos + 2] == ':' && input[pos + 3] == ':') {
        
        tok->type = TOK_MATRIX_START;
        tok->start = pos;
        
        // Find the end of the matrix definition
        uint32_t end = pos + 4;
        int bracket_depth = 1;
        
        while (end < len && bracket_depth > 0) {
            if (input[end] == '[') {
                bracket_depth++;
            } else if (input[end] == ']') {
                bracket_depth--;
            }
            end++;
        }
        
        if (bracket_depth == 0) {
            tok->len = end - pos;
            return end;
        }
    }
    
    return 0;
}

// Parse comparison operators: *>, *_<, *=, *!=
static uint32_t parse_comparison(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    if (input[pos] == '*') {
        if (pos + 1 < len && input[pos + 1] == '>') {
            tok->type = TOK_GREATER_THAN;
            tok->len = 2;
            return pos + 2;
        }
        if (pos + 1 < len && input[pos + 1] == '=') {
            tok->type = TOK_EQUAL;
            tok->len = 2;
            return pos + 2;
        }
        if (pos + 2 < len && input[pos + 1] == '_' && input[pos + 2] == '<') {
            tok->type = TOK_LESS_EQUAL;
            tok->len = 3;
            return pos + 3;
        }
        if (pos + 2 < len && input[pos + 1] == '!' && input[pos + 2] == '=') {
            tok->type = TOK_NOT_EQUAL;
            tok->len = 3;
            return pos + 3;
        }
    }
    return 0;
}

// Parse conditional: f.xxx or fucn.xxx
static uint32_t parse_conditional(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    bool is_short = false;
    uint32_t prefix_len = 0;
    
    if (match_string(input, pos, len, "f.")) {
        is_short = true;
        prefix_len = 2;
    } else if (match_string(input, pos, len, "fucn.")) {
        is_short = false;
        prefix_len = 5;
    } else {
        return 0;
    }
    
    // Check what follows
    uint32_t abbr_start = pos + prefix_len;
    
    // Match abbreviations
    struct {
        const char* abbr;
        TokenType type;
        uint32_t len;
    } conditionals[] = {
        {"ens", TOK_COND_ENS, 3},
        {"ver", TOK_COND_VER, 3},
        {"chk", TOK_COND_CHK, 3},
        {"try", TOK_COND_TRY, 3},
        {"grd", TOK_COND_GRD, 3},
        {"unl", TOK_COND_UNL, 3},
        {"whl", TOK_COND_WHL, 3},
        {"for", TOK_COND_FOR, 3},
        {"unt", TOK_COND_UNT, 3},
        {"obs", TOK_COND_OBS, 3},
        {"det", TOK_COND_DET, 3},
        {"rec", TOK_COND_REC, 3},
        {"rte", TOK_COND_RTE, 3},
        {"mon", TOK_COND_MON, 3},
        {"dec", TOK_COND_DEC, 3},
        {"ass", TOK_COND_ASS, 3},
        {"msr", TOK_COND_MSR, 3},
        {"eval", TOK_COND_EVAL, 4},
        {"if", TOK_COND_IF, 2},
        {"fs", TOK_COND_FS, 2},
    };
    
    for (int i = 0; i < sizeof(conditionals) / sizeof(conditionals[0]); i++) {
        if (match_string(input, abbr_start, len, conditionals[i].abbr)) {
            tok->type = conditionals[i].type;
            tok->len = prefix_len + conditionals[i].len;
            return pos + tok->len;
        }
    }
    
    return 0;
}

// Parse function call: verb.can/{@param:value}/ or verb.can/
static uint32_t parse_function_call(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    // Function calls in Blaze follow the pattern: identifier.identifier/.../ 
    uint32_t start = pos;
    
    // Need an identifier
    if (!is_alpha(input[pos])) return 0;
    
    // Parse first part (verb)
    while (pos < len && (is_alnum(input[pos]) || input[pos] == '_')) {
        pos++;
    }
    
    // Must have a dot
    if (pos >= len || input[pos] != '.') return 0;
    pos++; // Skip dot
    
    // Parse second part (can/method)
    if (pos >= len || !is_alpha(input[pos])) return 0;
    
    while (pos < len && (is_alnum(input[pos]) || input[pos] == '_')) {
        pos++;
    }
    
    // Must have a slash
    if (pos >= len || input[pos] != '/') return 0;
    
    // This is a function call pattern
    tok->type = TOK_FUNC_CALL;
    tok->len = pos - start;
    return pos;
}

// Parse string literal enclosed in double quotes
static uint32_t parse_string_literal(const char* input, uint32_t pos, uint32_t len, Token* tok) {
    if (pos >= len || input[pos] != '"') return 0;
    
    uint32_t start = pos;
    pos++; // Skip opening quote
    
    // Find closing quote
    while (pos < len && input[pos] != '"') {
        if (input[pos] == '\\' && pos + 1 < len) {
            pos += 2; // Skip escaped character
        } else {
            pos++;
        }
    }
    
    if (pos >= len) {
        // Unterminated string
        return 0;
    }
    
    pos++; // Skip closing quote
    
    tok->type = TOK_STRING;
    tok->len = pos - start;
    return pos;
}

// Main lexer function
uint32_t lex_blaze(const char* input, uint32_t len, Token* output) {
    print_str("[LEXER] ENTERED lex_blaze\n");
    uint32_t pos = 0;
    uint32_t token_count = 0;
    uint32_t line = 1;
    uint32_t next_pos = 0;  // Add missing variable declaration
    
    while (pos < len && token_count < MAX_TOKENS - 1) {
        print_str("[LEXER] MAIN LOOP at pos "); print_num(pos); print_str(", char='");
        if (pos < len) {
            char c = input[pos];
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
        
        // Skip whitespace
        pos = skip_whitespace(input, pos, len, &line);
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
        
        // Check for parameter/action block first
        if (pos < len && input[pos] == '<') {
            print_str("[LEXER] FOUND < at pos "); print_num(pos); print_str(", next 20 chars: '");
            for (int i = 0; i < 20 && pos + i < len; i++) {
                char c = input[pos + i];
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
            print_str("[LEXER] ENTERED < PARAM/ACTION BLOCK at pos "); print_num(pos); print_str("\n");
            
            uint32_t saved_pos = pos;  // Save original position
            pos++; // consume '<'
            pos = skip_whitespace(input, pos, len, &line);
            uint32_t new_pos = skip_comment(input, pos, len);
            if (new_pos != pos) pos = new_pos;
            
            bool found_param_or_action = false;
            
            // Parse zero or more parameters
            while ((next_pos = parse_parameter_after_lt(input, pos, len, &output[token_count])) != 0) {
                print_str("[LEXER] Found parameter token at pos "); print_num(pos); print_str("\n");
                output[token_count].start = pos;
                pos = next_pos;
                token_count++;
                found_param_or_action = true;
                pos = skip_whitespace(input, pos, len, &line);
                new_pos = skip_comment(input, pos, len);
                if (new_pos != pos) pos = new_pos;
            }
            
            // Always skip whitespace/comments before action block, even if no params
            pos = skip_whitespace(input, pos, len, &line);
            new_pos = skip_comment(input, pos, len);
            if (new_pos != pos) pos = new_pos;
            print_str("[LEXER] Before parse_action, next 10 chars: '");
            for (int i = 0; i < 10 && pos + i < len; i++) {
                char c = input[pos + i];
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
            if ((next_pos = parse_action(input, pos, len, &output[token_count])) != 0) {
                print_str("[LEXER] Found TOK_ACTION_START after < at pos "); print_num(pos); print_str("\n");
                output[token_count].start = pos;
                pos = next_pos;
                token_count++;
                found_param_or_action = true;
                continue;
            }
            
            // If we didn't find any parameters or actions, reset position and treat < as TOK_LT
            if (!found_param_or_action) {
                print_str("[LEXER] No param/action found, treating < as TOK_LT\n");
                pos = saved_pos;  // Reset to original position
                // Fall through to single-character token handler
            } else {
                continue;  // We processed parameter/action block successfully
            }
        }
        // Now check for temporal op if parameter/action block did not match
        if ((next_pos = parse_temporal_op(input, pos, len, tok)) != 0) {
            pos = next_pos;
            token_count++;
            continue;
        }
        
        // Try block end marker :>
        if ((next_pos = parse_block_end(input, pos, len, tok)) != 0) {
            pos = next_pos;
            token_count++;
            continue;
        }
        
        // Note: Function definitions like |name| are handled by parsing individual | tokens
        // and identifiers separately, not as a single token
        
        // Try variable declaration
        if ((next_pos = parse_var_decl(input, pos, len, tok)) != 0) {
            pos = next_pos;
            token_count++;
            continue;
        }
        
        // Try output methods: print/, txt/, out/, fmt/, dyn/, asm/
        if ((next_pos = parse_output_method(input, pos, len, tok)) != 0) {
            print_str("[LEXER] Found output method at pos "); print_num(pos); print_str(" type="); print_num(tok->type); print_str("\n");
            pos = next_pos;
            token_count++;
            continue;
        }
        
        // Try conditional: f.whl, f.for, f.if, etc.
        if ((next_pos = parse_conditional(input, pos, len, tok)) != 0) {
            print_str("[LEXER] Found conditional at pos "); print_num(pos); print_str(" type="); print_num(tok->type); print_str("\n");
            pos = next_pos;
            token_count++;
            continue;
        }
        
        // Try identifier
        if ((next_pos = parse_identifier(input, pos, len, tok)) != 0) {
            pos = next_pos;
            token_count++;
            continue;
        }
        
        // Try number
        if ((next_pos = parse_number(input, pos, len, tok)) != 0) {
            pos = next_pos;
            token_count++;
            continue;
        }
        
        // Try string literal
        if ((next_pos = parse_string_literal(input, pos, len, tok)) != 0) {
            pos = next_pos;
            token_count++;
            continue;
        }
        
        // Try single character tokens
        if (pos < len) {
            char c = input[pos];
            switch (c) {
                case '<':
                    print_str("[LEXER] SINGLE-CHAR < token at pos "); print_num(pos); print_str("\n");
                    tok->type = TOK_LT;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '>':
                    tok->type = TOK_GT;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '/':
                    // Check for /=
                    if (pos + 1 < len && input[pos + 1] == '=') {
                        tok->type = TOK_DIV_EQUAL;
                        tok->len = 2;
                        pos += 2;
                        token_count++;
                        continue;
                    }
                    tok->type = TOK_DIV;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '\\':
                    tok->type = TOK_BACKSLASH;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '|':
                    tok->type = TOK_PIPE;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '[':
                    tok->type = TOK_BRACKET_OPEN;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case ']':
                    tok->type = TOK_BRACKET_CLOSE;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '^':
                    tok->type = TOK_JUMP_MARKER;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '!':
                    tok->type = TOK_BANG;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '.':
                    tok->type = TOK_DOT;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '_':
                    tok->type = TOK_UNDERSCORE;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '@':
                    tok->type = TOK_AT;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case ';':
                    tok->type = TOK_SEMICOLON;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '%':
                    // Check for %=
                    if (pos + 1 < len && input[pos + 1] == '=') {
                        tok->type = TOK_PERCENT_EQUAL;
                        tok->len = 2;
                        pos += 2;
                        token_count++;
                        continue;
                    }
                    tok->type = TOK_PERCENT;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '=':
                    tok->type = TOK_EQUALS;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '(':
                    tok->type = TOK_LPAREN;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case ')':
                    tok->type = TOK_RPAREN;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '{':
                    tok->type = TOK_LBRACE;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '}':
                    tok->type = TOK_RBRACE;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case ':':
                    tok->type = TOK_COLON;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '?':
                    tok->type = TOK_QUESTION;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '-':
                    // Check for -- or -=
                    if (pos + 1 < len) {
                        if (input[pos + 1] == '-') {
                            tok->type = TOK_DECREMENT;
                            tok->len = 2;
                            pos += 2;
                            token_count++;
                            continue;
                        } else if (input[pos + 1] == '=') {
                            tok->type = TOK_MINUS_EQUAL;
                            tok->len = 2;
                            pos += 2;
                            token_count++;
                            continue;
                        }
                    }
                    tok->type = TOK_MINUS;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '*':
                    // Check for ** or *=
                    if (pos + 1 < len) {
                        if (input[pos + 1] == '*') {
                            // Check for **= (exponentiation assignment)
                            if (pos + 2 < len && input[pos + 2] == '=') {
                                tok->type = TOK_EXPONENT_EQUAL;
                                tok->len = 3;
                                pos += 3;
                                token_count++;
                                continue;
                            }
                            tok->type = TOK_EXPONENT;
                            tok->len = 2;
                            pos += 2;
                            token_count++;
                            continue;
                        } else if (input[pos + 1] == '=') {
                            tok->type = TOK_STAR_EQUAL;
                            tok->len = 2;
                            pos += 2;
                            token_count++;
                            continue;
                        }
                    }
                    tok->type = TOK_STAR;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case ',':
                    tok->type = TOK_COMMA;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '+':
                    // Check for ++ or +=
                    if (pos + 1 < len) {
                        if (input[pos + 1] == '+') {
                            tok->type = TOK_INCREMENT;
                            tok->len = 2;
                            pos += 2;
                            token_count++;
                            continue;
                        } else if (input[pos + 1] == '=') {
                            tok->type = TOK_PLUS_EQUAL;
                            tok->len = 2;
                            pos += 2;
                            token_count++;
                            continue;
                        }
                    }
                    tok->type = TOK_PLUS;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                case '#':
                    tok->type = TOK_COMMENT;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
                default:
                    if (c == '<') {
                        print_str("[LEXER] DEFAULT CASE: treating '<' as single-char token at pos ");
                        print_num(pos);
                        print_str("\n");
                    }
                    print_str("[LEXER] Unknown character '");
                    char buf[2] = {c, '\0'};
                    print_str(buf);
                    print_str("' at pos "); print_num(pos); print_str("\n");
                    tok->type = TOK_ERROR;
                    tok->len = 1;
                    pos++;
                    token_count++;
                    continue;
            }
        }
        
        // If we get here, we couldn't parse anything
        print_str("[LEXER] Failed to parse at pos "); print_num(pos); print_str("\n");
        tok->type = TOK_ERROR;
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
    
    uint32_t result = token_count;
    // Debug: print all tokens and their text
    print_str("[LEXER DEBUG] All tokens:\n");
    for (uint32_t i = 0; i < result; i++) {
        Token* t = &output[i];
        print_str("  token["); print_num(i); print_str("]: type="); print_num(t->type);
        print_str(" start="); print_num(t->start);
        print_str(" len="); print_num(t->len);
        print_str(" text='");
        for (uint32_t j = 0; j < t->len; j++) {
            char c = input[t->start + j];
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
    }
    
    print_str("[LEXER] EXITING lex_blaze\n");
    return token_count;
}

// Debug function to print tokens
void debug_print_tokens(Token* tokens, uint32_t count, const char* source) {
    print_str("\n=== TOKENS ===\n");
    
    for (uint32_t i = 0; i < count && tokens[i].type != TOK_EOF; i++) {
        Token* t = &tokens[i];
        
        print_str("Line ");
        print_num(t->line);
        print_str(": ");
        
        // Print token type
        switch (t->type) {
            case TOK_VAR: print_str("VAR"); break;
            case TOK_CONST: print_str("CONST"); break;
            case TOK_ARRAY_4D: print_str("ARRAY_4D"); break;
            case TOK_GAP_COMPUTE: print_str("GAP_COMPUTE"); break;
            case TOK_PARAM: print_str("PARAM"); break;
            case TOK_MATRIX_START: print_str("MATRIX"); break;
            case TOK_TIMELINE_DEF: print_str("TIMELINE_DEF"); break;
            case TOK_TIMELINE_JUMP: print_str("TIMELINE_JUMP"); break;
            case TOK_ACTION_START: print_str("ACTION_START"); break;
            case TOK_ACTION_END: print_str("ACTION_END"); break;
            case TOK_CONNECTOR_FWD: print_str("CONN_FWD"); break;
            case TOK_CONNECTOR_BWD: print_str("CONN_BWD"); break;
            case TOK_TIMING_ONTO: print_str("ONTO"); break;
            case TOK_TIMING_INTO: print_str("INTO"); break;
            case TOK_TIMING_BOTH: print_str("BOTH"); break;
            case TOK_LT: print_str("LT"); break;
            case TOK_GT: print_str("GT"); break;
            case TOK_BLOCK_END: print_str("BLOCK_END"); break;
            case TOK_TIME_BRIDGE_FWD: print_str("TIME_BRIDGE_FWD"); break;
            case TOK_SLOW_FWD: print_str("SLOW_FWD"); break;
            case TOK_FAST_REWIND: print_str("FAST_REWIND"); break;
            case TOK_SLOW_REWIND: print_str("SLOW_REWIND"); break;
            case TOK_GREATER_THAN: print_str("GREATER_THAN"); break;
            case TOK_LESS_EQUAL: print_str("LESS_EQUAL"); break;
            case TOK_EQUAL: print_str("EQUAL"); break;
            case TOK_NOT_EQUAL: print_str("NOT_EQUAL"); break;
            case TOK_COND_CHK: print_str("COND_CHK"); break;
            case TOK_COND_ENS: print_str("COND_ENS"); break;
            case TOK_COND_VER: print_str("COND_VER"); break;
            case TOK_COND_IF: print_str("COND_IF"); break;
            case TOK_BNC: print_str("BNC"); break;
            case TOK_RECV: print_str("RECV"); break;
            case TOK_FIX_P: print_str("FIX_P"); break;
            case TOK_F_P: print_str("F_P"); break;
            case TOK_TIMELINE_PER: print_str("TIMELINE_PER"); break;
            case TOK_TIMELINE_P: print_str("TIMELINE_P"); break;
            case TOK_TIMELINE_P_JUMP: print_str("TIMELINE_P_JUMP"); break;
            case TOK_PRINT: print_str("PRINT"); break;
            case TOK_TXT: print_str("TXT"); break;
            case TOK_OUT: print_str("OUT"); break;
            case TOK_FMT: print_str("FMT"); break;
            case TOK_DYN: print_str("DYN"); break;
            case TOK_ASM: print_str("ASM"); break;
            case TOK_IDENTIFIER: print_str("IDENT"); break;
            case TOK_NUMBER: print_str("NUMBER"); break;
            case TOK_STRING: print_str("STRING"); break;
            case TOK_PIPE: print_str("PIPE"); break;
            case TOK_SLASH: print_str("SLASH"); break;
            case TOK_BACKSLASH: print_str("BACKSLASH"); break;
            case TOK_JUMP_MARKER: print_str("JUMP"); break;
            case TOK_MINUS: print_str("MINUS"); break;
            case TOK_BRACKET_OPEN: print_str("LBRACKET"); break;
            case TOK_BRACKET_CLOSE: print_str("RBRACKET"); break;
            case TOK_DOT: print_str("DOT"); break;
            case TOK_SEMICOLON: print_str("SEMICOLON"); break;
            case TOK_COLON: print_str("COLON"); break;
            case TOK_LBRACE: print_str("LBRACE"); break;
            case TOK_RBRACE: print_str("RBRACE"); break;
            case TOK_PLUS: print_str("PLUS"); break;
            case TOK_STAR: print_str("STAR"); break;
            case TOK_PERCENT: print_str("PERCENT"); break;
            case TOK_EXPONENT: print_str("EXPONENT"); break;
            case TOK_PLUS_EQUAL: print_str("PLUS_EQUAL"); break;
            case TOK_MINUS_EQUAL: print_str("MINUS_EQUAL"); break;
            case TOK_STAR_EQUAL: print_str("STAR_EQUAL"); break;
            case TOK_DIV_EQUAL: print_str("DIV_EQUAL"); break;
            case TOK_PERCENT_EQUAL: print_str("PERCENT_EQUAL"); break;
            case TOK_EXPONENT_EQUAL: print_str("EXPONENT_EQUAL"); break;
            case TOK_INCREMENT: print_str("INCREMENT"); break;
            case TOK_DECREMENT: print_str("DECREMENT"); break;
            case TOK_QUESTION: print_str("QUESTION"); break;
            case TOK_EOF: print_str("EOF"); break;
            default: 
                print_str("TOK(");
                print_num(t->type);
                print_str(")");
                break;
        }
        
        print_str(" \"");
        // Print actual token text
        for (uint32_t j = 0; j < t->len && j < 30; j++) {
            if (source[t->start + j] == '\n') {
                print_str("\\n");
            } else {
                char c[2] = {source[t->start + j], '\0'};
                print_str(c);
            }
        }
        if (t->len > 30) print_str("...");
        print_str("\"\n");
    }
    
    print_str("=== END TOKENS ===\n");
}