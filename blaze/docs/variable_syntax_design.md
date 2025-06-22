# Blaze Variable Syntax Design

## Syntax Pattern
`var.type-name-[value]`

## Type Prefixes

| Prefix | Type | Description | Example |
|--------|------|-------------|---------|
| `v` | Variable | Generic mutable variable | `var.v-data-[42]` |
| `c` | Constant | Immutable value | `var.c-PI-[3.14159]` |
| `i` | Integer | Whole numbers | `var.i-count-[10]` |
| `f` | Float | Decimal numbers | `var.f-price-[19.99]` |
| `s` | String | Text values | `var.s-name-[Alice]` |
| `b` | Boolean | True/false values | `var.b-active-[true]` |

## Benefits

1. **Type Safety**: Type is explicit in declaration
2. **Readability**: Clear what kind of data is stored
3. **Optimization**: Compiler can optimize based on type
4. **Error Detection**: Type mismatches caught at compile time

## Usage Examples

```blaze
# Constants
var.c-MAX_SIZE-[100]
var.c-PI-[3.14159]

# Integers
var.i-age-[25]
var.i-count-[0]

# Strings
var.s-greeting-[Hello World]
var.s-username-[alice123]

# Booleans
var.b-isReady-[false]
var.b-hasPermission-[true]

# Floats
var.f-temperature-[98.6]
var.f-balance-[1234.56]

# Generic variables (when type isn't specified)
var.v-temp-[unknown]
```

## Implementation Notes

1. **Lexer**: Must tokenize the type prefix separately
2. **Parser**: Create AST nodes with type information
3. **Code Generation**: Generate appropriate assembly based on type
4. **Type Checking**: Validate operations match variable types