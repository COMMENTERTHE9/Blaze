# Solid Number Syntax Design for Blaze

## Quick Syntax for Common Cases

### 1. Exact Numbers (No Uncertainty)
```blaze
# Full syntax
var.x- 42...(exact)...42

# Quick syntax - use ! suffix
var.x- 42!
```

### 2. Quantum Barrier Numbers
```blaze
# Full syntax
var.pi- 3.14159...(q:10^35|0.85)...26535

# Quick syntax - use ~ for quantum with default confidence
var.pi- 3.14159~26535

# With custom confidence (0-100%)
var.pi- 3.14159~85%~26535
```

### 3. Infinity
```blaze
# Full syntax
var.inf- ...(∞:∞)...{*}

# Quick syntax
var.inf- ∞
```

### 4. Natural Numbers (ℕ)
```blaze
# Quick syntax
var.n- ℕ
```

### 5. Undefined
```blaze
# Quick syntax
var.u- ∅
```

## Variable Declaration with Type Inference

```blaze
# Solid number variables
var.d-pi- 3.14159~26535     # 'd' for solid (double-like)
var.d-exact- 42!             # Exact solid
var.d-inf- ∞                 # Infinity solid

# Mixed arithmetic automatically promotes to solid
var.x- 5                     # Regular int
var.y- 3.14159~26535         # Solid number
var.z- x + y                 # Result is solid
```

## Function Support

```blaze
# Function that returns solid number
|calc_pi| verb.can<
    # Compute pi with quantum barrier
    var.d-result- 3.14159265358979323846~...~64338
    do.return result
:>

# Function with solid parameters
|add_solid| verb.can<
    param.a
    param.b
    do.return a + b
:>

# Call with solid numbers
var.pi- ^calc_pi/
var.sum- ^add_solid/ 3.14~159, 2.71~828
```

## Arithmetic Operations

```blaze
# All operations work with solids
var.a- 3.14159~26535
var.b- 2.71828~18284

var.sum- a + b          # Gap propagation
var.diff- a - b         # Gap propagation
var.prod- a * b         # Terminal multiplication
var.quot- a / b         # Modular division

# Special cases
var.inf1- ∞
var.inf2- ∞
var.nat- inf1 - inf2    # Returns ℕ
```

## Solid Number Literals in Expressions

```blaze
# Direct use in expressions
print/ 3.14159~26535 + 2.71828~18284

# In conditionals
if/ x > 3.14159~26535
    print/ "Greater than pi"
:>

# In loops
var.d-epsilon- 0.00001!
while/ error > epsilon
    # Refine calculation
:>
```

## Conversion Functions

```blaze
# Convert regular number to exact solid
var.d-exact- solid.exact(42)

# Extract known digits
var.known- solid.known(pi)

# Get confidence level
var.conf- solid.confidence(pi)

# Check if exact
if/ solid.is_exact(x)
    print/ "No uncertainty"
:>
```

## Pattern Matching on Barrier Types

```blaze
# Match on barrier type
match/ solid.barrier(x)
    case/ 'q'   # Quantum
        print/ "Quantum barrier"
    case/ 'e'   # Energy
        print/ "Energy barrier"
    case/ 'x'   # Exact
        print/ "Exact number"
    case/ '∞'   # Infinity
        print/ "Infinite"
:>
```

## Implementation Priority

1. **Phase 1**: Quick syntax for exact (!) and quantum (~)
2. **Phase 2**: Variable type 'd' for solids
3. **Phase 3**: Arithmetic operations
4. **Phase 4**: Function parameters and returns
5. **Phase 5**: Conversion functions
6. **Phase 6**: Pattern matching

This design makes solid numbers easy to use while preserving the full power of the notation when needed.