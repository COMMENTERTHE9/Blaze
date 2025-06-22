# Solid Numbers Syntax Specification for Blaze

## 1. Lexical Grammar

### 1.1 Token Definitions

```
SOLID_KNOWN_DIGITS  := DIGIT+ ('.' DIGIT+)?
SOLID_ELLIPSIS      := '...'
SOLID_LPAREN        := '('
SOLID_RPAREN        := ')'
SOLID_BARRIER_TYPE  := 'q' | 'e' | 's' | 't' | 'c' | '∞' | 'u'
SOLID_EXACT         := 'exact'
SOLID_COLON         := ':'
SOLID_PIPE          := '|'
SOLID_TERMINAL      := DIGIT{1,8} | '∅' | '{*}'
SOLID_GAP_MAG       := '10' ('^' DIGIT+ | SUPERSCRIPT+) | '∞'

SUPERSCRIPT := '⁰' | '¹' | '²' | '³' | '⁴' | '⁵' | '⁶' | '⁷' | '⁸' | '⁹'
```

### 1.2 Lexer States

```
STATE_NORMAL        → recognize SOLID_KNOWN_DIGITS
                   → on '...', transition to STATE_SOLID_GAP

STATE_SOLID_GAP    → expect '('
                   → transition to STATE_SOLID_BARRIER

STATE_SOLID_BARRIER → recognize SOLID_BARRIER_TYPE or SOLID_EXACT
                   → if SOLID_EXACT, expect ')', goto STATE_SOLID_TERMINAL_START
                   → else expect ':', goto STATE_SOLID_GAP_MAG

STATE_SOLID_GAP_MAG → recognize SOLID_GAP_MAG
                   → optionally expect '|', goto STATE_SOLID_CONFIDENCE
                   → else expect ')', goto STATE_SOLID_TERMINAL_START

STATE_SOLID_CONFIDENCE → recognize FLOAT (0.0 to 1.0)
                      → expect ')', goto STATE_SOLID_TERMINAL_START

STATE_SOLID_TERMINAL_START → expect '...'
                          → transition to STATE_SOLID_TERMINAL

STATE_SOLID_TERMINAL → recognize SOLID_TERMINAL
                    → return complete SOLID_NUMBER token
```

## 2. Syntactic Grammar

### 2.1 BNF Grammar

```bnf
solid_number ::= known_part gap_spec terminal_part

known_part ::= SOLID_KNOWN_DIGITS

gap_spec ::= '...' '(' barrier_spec ')'

barrier_spec ::= exact_spec | limit_spec

exact_spec ::= 'exact'

limit_spec ::= SOLID_BARRIER_TYPE ':' SOLID_GAP_MAG confidence_opt

confidence_opt ::= ε | '|' confidence_value

confidence_value ::= FLOAT  // 0.0 to 1.0

terminal_part ::= '...' SOLID_TERMINAL
```

### 2.2 Precedence and Associativity

Solid numbers have the same precedence as regular number literals in expressions.

## 3. Blaze Language Integration

### 3.1 Variable Declaration

```blaze
// Type inference from literal
solid.pi = 3.14159...(q:10³⁵|0.85)...787

// Explicit solid type
solid.var-name-[initial_value]

// Mixed with regular variables
var.f-radius-[2.0]
solid.area = pi * radius * radius
```

### 3.2 Solid Number Literals

```blaze
// Full notation
3.14159...(q:10³⁵|0.85)...787

// Without confidence (defaults to 1.0)
3.14159...(q:10³⁵)...787

// Exact numbers
0.5...(exact)...000
1.0...(exact)...000

// Special terminals
2.718...(e:10⁵⁰|0.9)...∅     // Undefined terminal
...(∞:∞|1.0)...{*}           // Infinity
```

### 3.3 Operations

```blaze
// Arithmetic operations
solid.sum = a + b
solid.diff = a - b
solid.prod = a * b
solid.quot = a / b

// Mixed operations (solid + regular)
solid.result = solid_num * 2.0  // 2.0 promoted to exact solid
```

## 4. Lexer Implementation Details

### 4.1 Character Encoding

- Support UTF-8 for special characters (∞, ∅, superscripts)
- Provide ASCII alternatives:
  - `∞` can be written as `inf`
  - `∅` can be written as `null` or `empty`
  - Superscripts can use `^` notation

### 4.2 Whitespace Handling

```blaze
// Whitespace allowed in these positions only:
3.14159 ... ( q : 10³⁵ | 0.85 ) ... 787  // OK
3.14159... (q:10³⁵|0.85) ...787          // OK
3.14159...(q:10³⁵|0.85)...787            // OK (preferred)

// Not allowed:
3.14 159...(q:10³⁵)...787                // Error in known digits
3.14159...(q: 10 ³⁵)...787              // Error in exponent
```

### 4.3 Lexer Pseudocode

```c
Token* lex_solid_number() {
    Token* tok = create_token(TOK_SOLID_NUMBER);
    
    // Parse known digits
    while (is_digit(current) || current == '.') {
        append_to_buffer(known_buffer, current);
        advance();
    }
    tok->known_digits = strdup(known_buffer);
    
    // Expect first ellipsis
    if (!match_string("...")) {
        return error("Expected '...' after known digits");
    }
    
    // Expect '('
    if (!match('(')) {
        return error("Expected '(' after '...'");
    }
    
    // Check for exact
    if (match_string("exact")) {
        tok->barrier_type = 'x';
        tok->gap_magnitude = 0;
        tok->confidence = 1.0;
    } else {
        // Parse barrier type
        if (match_any("qestc∞u")) {
            tok->barrier_type = previous();
        } else {
            return error("Invalid barrier type");
        }
        
        // Expect ':'
        if (!match(':')) {
            return error("Expected ':' after barrier type");
        }
        
        // Parse gap magnitude
        tok->gap_magnitude = parse_gap_magnitude();
        
        // Optional confidence
        if (match('|')) {
            tok->confidence = parse_float();
            if (tok->confidence < 0.0 || tok->confidence > 1.0) {
                return error("Confidence must be between 0 and 1");
            }
        } else {
            tok->confidence = 1.0;
        }
    }
    
    // Expect ')'
    if (!match(')')) {
        return error("Expected ')' to close barrier spec");
    }
    
    // Expect second ellipsis
    if (!match_string("...")) {
        return error("Expected '...' before terminal");
    }
    
    // Parse terminal
    if (match_string("∅") || match_string("null")) {
        tok->terminal_type = TERM_UNDEFINED;
    } else if (match_string("{*}")) {
        tok->terminal_type = TERM_SUPERPOSITION;
    } else {
        tok->terminal_type = TERM_DIGITS;
        tok->terminal = parse_terminal_digits();
    }
    
    return tok;
}
```

## 5. Parser Integration

### 5.1 AST Node Structure

```c
typedef enum {
    NODE_INT,
    NODE_FLOAT,
    NODE_SOLID,  // New node type
    // ... other types
} NodeType;

typedef struct {
    uint32_t known_offset;     // String pool offset
    uint16_t known_len;
    char barrier_type;         // 'q','e','s','t','c','∞','u','x'
    uint64_t gap_magnitude;
    uint16_t confidence_x1000; // Store as integer (0-1000)
    uint32_t terminal_offset;  // String pool offset
    uint8_t terminal_len;
    uint8_t terminal_type;     // 0=digits, 1=undefined, 2=superposition
} SolidData;
```

### 5.2 Parser Rules

```c
// In parse_primary()
case TOK_SOLID_NUMBER: {
    uint16_t node = alloc_node(p, NODE_SOLID);
    Node* n = &p->nodes[node];
    
    // Store known digits in string pool
    n->data.solid.known_offset = add_to_string_pool(
        p->string_pool, 
        token->known_digits
    );
    n->data.solid.known_len = strlen(token->known_digits);
    
    // Copy barrier info
    n->data.solid.barrier_type = token->barrier_type;
    n->data.solid.gap_magnitude = token->gap_magnitude;
    n->data.solid.confidence_x1000 = (uint16_t)(token->confidence * 1000);
    
    // Store terminal
    n->data.solid.terminal_offset = add_to_string_pool(
        p->string_pool,
        token->terminal
    );
    n->data.solid.terminal_len = token->terminal_len;
    n->data.solid.terminal_type = token->terminal_type;
    
    advance(p);  // Consume token
    return node;
}
```

## 6. Validation Rules

### 6.1 Lexical Validation

1. **Known digits**: Must be valid decimal number
2. **Gap magnitude**: Must be `10^n` where n > 0, or `∞`
3. **Confidence**: Must be in range [0.0, 1.0]
4. **Terminal length**: Typically 3, max 8 digits
5. **Terminal digits**: Must be valid digits or special values

### 6.2 Semantic Validation

1. **Exact numbers**: Should have terminal `...000`
2. **Infinity**: Must have barrier `∞` and terminal `{*}`
3. **Gap magnitude**: Should be less than 10^1000 (practical limit)
4. **Confidence**: Warn if initial confidence < 0.5

## 7. Error Messages

```
ERROR: Expected '...' after known digits in solid number
ERROR: Invalid barrier type 'x' (expected q,e,s,t,c,∞,u)
ERROR: Missing gap magnitude after ':'
ERROR: Invalid confidence value 1.5 (must be 0.0 to 1.0)
ERROR: Terminal too long (max 8 digits)
ERROR: Unexpected character in terminal digits
```

## 8. Examples of Valid Syntax

```blaze
// Standard notation
3.14159...(q:10³⁵|0.85)...787

// Using ASCII alternatives
3.14159...(q:10^35|0.85)...787

// Without confidence
2.71828...(e:10⁵⁰)...123

// Exact number
0.5...(exact)...000

// Infinity
...(inf:inf|1.0)...{*}

// Undefined terminal
1.41421...(c:10²⁰|0.79)...null

// Very large gap
1.23456...(s:10^82|0.91)...456

// Single terminal digit
9.999...(t:10¹²|0.5)...7
```

## 9. Implementation Phases

### Phase 1: Basic Lexing
- Recognize solid number tokens
- Support ASCII alternatives
- Basic error reporting

### Phase 2: Full Unicode Support
- Handle ∞, ∅, superscripts
- UTF-8 parsing

### Phase 3: Parser Integration
- AST node creation
- String pool integration
- Type checking

### Phase 4: Optimization
- Token caching
- Fast path for common patterns
- Minimal memory allocation