# Blaze Punctuation: The Complete Deep Dive

Every single punctuation mark in Blaze tells you something. This isn't decoration - it's information.

## The Forward Slash `/` - Command Separator

The `/` is a **command separator** - it separates the action/command from its parameters. It's NOT a flow operator.

### In Function Calls
```blaze
^function_name/          # / separates function name from call
^function_name/ param    # Parameters come after the /
```

### In Statements
```blaze
if/ condition <          # / separates 'if' from condition
print/ "Hello" \         # / separates 'print' from what to print
while/ x < 10 <          # / separates 'while' from condition
```

Think of `/` as a colon or separator - "do this / with this"

## Flow Operators - The Real Flow Syntax

### Flow Direction `<` `>` 
```blaze
data > variable          # Data flows INTO variable
variable < data          # Variable receives data
```

### Pipeline Flow `<<` `>>`
```blaze
input >> process >> output    # Pipeline flow left to right
output << process << input    # Pipeline flow right to left
```

### Bidirectional Flow `</<`
```blaze
a </< b                  # Bidirectional data exchange
```

## The Backslash `\` - Statement Termination

The `\` is the **hard stop** for statements that produce output or effects.

```blaze
print/ "Hello" \         # STOP - statement complete
var.v-x-[10]            # No \ needed - declaration, not output
x = x + 1               # No \ needed - assignment, not output
print/ x \              # STOP - output statement needs \
```

Why? Because output is a **boundary** between your program and the outside world. The `\` marks that boundary clearly.

## The Pipes `|...|` - Containment Boundaries

Pipes create **visual walls** showing containment:

```blaze
|function_name| verb.can<    # Function name contained in pipes
    # function body
:>

|complex_name_here| func.can<  # Even long names are visually bounded
    # code
:>
```

This isn't just aesthetics. Your brain instantly sees the function boundaries without parsing text.

## Angle Brackets `<` and `:>` - Scope Delimiters

### Opening `<`
The `<` opens a new scope - think "less than" pointing into the scope:

```blaze
if/ x > 5 <              # Opens if scope
|func| verb.can<         # Opens function scope
while/ true <            # Opens loop scope
```

### Closing `:>`
The `:>` closes scope - the colon groups with the angle to make a distinct closing marker:

```blaze
if/ x > 5 <
    print/ "Big" \
:>                       # Clearly different from > operator
```

Why `:>`? Because `>` alone would be ambiguous with greater-than. The `:` makes it unambiguous.

## The Caret `^` - Function Invocation & Timeline Jumps

The `^` has two main uses:

### 1. Function Invocation
```blaze
^function_name/          # Invoke function
^math.calculate/ x y     # Invoke with parameters
```

### 2. Timeline Jumps
```blaze
^timeline.[checkpoint]/  # Jump to timeline checkpoint
^timeline.[past]/       # Jump to past
^timeline.[future]/     # Jump to future
```

The visual metaphor: `^` points up, like jumping up and over to another point.

## The Dot `.` - Namespace/Type Separation

The dot separates **categorical information**:

```blaze
var.v-name-[10]         # var (category) . v (type)
var.f-ratio-[3.14]      # var (category) . f (float type)
math.sin x              # math (namespace) . sin (function)
```

### In Bitwise Operators
The dot disambiguates operators:

```blaze
# Logical operators (no dot)
x && y                  # Logical AND
x || y                  # Logical OR

# Bitwise operators (with dot)
x &&. y                 # Bitwise AND - the . means "bits"
x ||. y                 # Bitwise OR
x <<. 2                 # Left shift bits
x >>. 1                 # Right shift bits
```

The dot literally means "operate on the bits/components".

## The Dash `-` - Multi-part Names

The dash creates **readable multi-part identifiers**:

```blaze
var.v-user-name-["John"]        # user-name is the identifier
var.f-circle-area-[pi * r * r]  # circle-area is the identifier
var.d-quantum-state-["DEAD!"]    # quantum-state is the identifier
```

Why not camelCase or snake_case? Because dashes create natural word boundaries your brain instantly parses.

## Square Brackets `[...]` - Value Containers

Square brackets **contain values**:

```blaze
var.v-x-[10]            # Contains initial value
var.v-sum-[a + b]       # Contains computed value
var.d-solid-["FF!"]     # Contains solid number value
```

They're not function calls or array indices - they're value containers.

## Special Solid Number Punctuation

### Exclamation `!` - Exact Barrier
```blaze
var.d-exact-["DEADBEEF!"]   # ! = no terminal digits allowed
```
The `!` is a hard barrier - nothing can follow.

### Tilde `~` - Quantum Barrier
```blaze
var.d-quantum-["3A7F~9B2C"]  # ~ = terminal digits follow
```
The `~` is a soft barrier - terminal quantum digits can follow.

## Parentheses `(...)` - Almost Never Used!

Blaze barely uses parentheses. When you do see them:

```blaze
# Grouping in complex expressions (rare)
var.v-result-[(a + b) * c]   # Only when precedence unclear

# NEVER for:
# - Function calls (use ^name/ instead)
# - Function definitions (use |name| instead)
# - Control structures (use keyword/ instead)
```

## The Philosophy: See The Structure

Traditional code:
```c
if (calculate(x, y) > process(a, b)) {
    output(format("Result: %d", value));
}
```

Blaze equivalent:
```blaze
if/ ^calculate/ x y > ^process/ a b <
    print/ "Result: " \
    print/ value \
:>
```

In Blaze you can see:
- `/` shows flow direction
- `^` shows function elevations
- `<` `:>` show scope boundaries
- `\` shows output boundaries

## Water Flow Operators

For the complete explanation of how `<` and `>` represent water flow direction, see [Water Flow Metaphor](WATER_METAPHOR_COMPLETE.md).

Key points:
- `<` and `>` show water direction (not covered here - see water metaphor doc)
- `*>` and `*<` are comparisons (not water flow)
- Time bridges like `>/>` control speed (covered in water metaphor doc)

## Why This Matters

1. **No Ambiguity**: Every symbol has ONE meaning
2. **Visual Parsing**: Structure visible without reading
3. **Flow Clarity**: See data/control flow at a glance
4. **Cognitive Load**: Less mental parsing needed
5. **Error Prevention**: Can't misuse syntax you can see

Once you internalize this, reading Blaze is like reading a diagram, not parsing text.