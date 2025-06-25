# Blaze 🔥

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/github/actions/workflow/status/COMMENTERTHE9/Blaze/test.yml?branch=main)](https://github.com/COMMENTERTHE9/Blaze/actions)
[![GitHub Issues](https://img.shields.io/github/issues/COMMENTERTHE9/Blaze)](https://github.com/COMMENTERTHE9/Blaze/issues)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](blaze/CONTRIBUTING.md)
[![Help Wanted](https://img.shields.io/badge/help-wanted-green.svg)](https://github.com/COMMENTERTHE9/Blaze/labels/help%20wanted)
[![Good First Issues](https://img.shields.io/github/issues/COMMENTERTHE9/Blaze/good%20first%20issue)](https://github.com/COMMENTERTHE9/Blaze/labels/good%20first%20issue)
[![GitHub Stars](https://img.shields.io/github/stars/COMMENTERTHE9/Blaze?style=social)](https://github.com/COMMENTERTHE9/Blaze)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20WSL-blue)](https://github.com/COMMENTERTHE9/Blaze)

A revolutionary programming language compiler that translates Blaze language to x86-64 machine code, featuring time-travel debugging, GGGX optimization, and industrial-strength computation support.

## Features

- **Direct Machine Code Generation**: Compiles directly to x86-64 assembly without intermediate representations
- **Time-Travel Debugging**: Built-in temporal flow operators for advanced debugging capabilities → [Learn more](blaze/docs/TIME_TRAVEL_SPEC.md)
- **Solid Numbers**: Revolutionary number representation with computational barriers → [Learn more](blaze/docs/SOLID_NUMBERS_IMPLEMENTATION.md)
- **GGGX Algorithm**: Gap-aware optimization for industrial computation → [Learn more](blaze/docs/GGGX_FRAMEWORK.md)
- **Temporal Memory Management**: Three-zone memory system (Past/Present/Future) with temporal-aware GC
- **Scalable Code Generation**: Supports output files >1MB with multi-segment architecture
- **Sentry Integration**: Automatic error tracking and reporting
- **SSE/Float Support**: Full floating-point arithmetic with SSE instructions
- **Math Functions**: sin, cos, tan, sqrt, log, exp built-in support
- **Bitwise Operations**: Conflict-free operators (&&., ||., ^^, ~~, <<., >>.)

## Building

### Prerequisites
- GCC or compatible C compiler
- Make
- Linux/Unix environment (WSL supported)

### Build Commands

```bash
# Standard build
make

# Debug build with Sentry integration
make debug

# Clean build artifacts
make clean
```

## Usage

```bash
# Compile a Blaze program
./blaze_compiler input.blaze output

# Run with debug build
./blaze_debug input.blaze output

# Execute compiled program
./output
```

## Blaze Language Syntax

### Variables
```blaze
# Basic syntax (short and full forms)
var.v-name-[value]          # Generic variable (or var.var-)
var.i-x-[10]                # Integer (or var.int-)
var.f-pi-[3.14159]          # Float (or var.float-)
var.s-msg-["Hello"]         # String (or var.string-)
var.b-flag-[true]           # Boolean (or var.bool-)
var.d-num-[42...(q:10³)...] # Solid number (or var.solid-)
var.c-PI-[3.14159]          # Constant (or var.const-)

# Examples
var.v-x-[10]
var.v-y-[x + 5]
var.f-circle_area-[pi * r * r]
```

### Functions
```blaze
# Function definition (func.can or fucn.can)
|function_name| func.can<
    var.v-local-[42]
    print/ local \
:>

# Function call
^function_name/

# Conditional functions (all support f./fucn./func. prefixes)
|validate| func.can<
    func.if x *> 10 <
        print/ "Large number" \
    :>
    func.ens <  # Ensure/else
        print/ "Small number" \
    :>
:>
```

### Print Statements
**Important:** All output statements require a closing backslash `\` to terminate the command.

```blaze
print/ "Hello, World!" \
print/ variable_name \
print/ x + y \

# Other output methods
txt/ "Plain text" \    # Text output
out/ result \         # Standard output
fmt/ "x = ", x \      # Formatted output
dyn/ data \           # Dynamic output
```

### Arithmetic Operations
```blaze
# Basic arithmetic
var.v-sum-[a + b]
var.v-diff-[a - b]
var.v-prod-[a * b]
var.v-quot-[a / b]
var.v-mod-[a % b]
var.v-pow-[a ** b]    # Exponentiation

# Bitwise operations (note the dots!)
var.v-and-[a &&. b]   # Bitwise AND
var.v-or-[a ||. b]    # Bitwise OR
var.v-xor-[a ^^ b]    # Bitwise XOR
var.v-not-[~~ a]      # Bitwise NOT
var.v-lshift-[a <<. b] # Left shift
var.v-rshift-[a >>. b] # Right shift

# Math functions
var.f-sine-[math.sin x]
var.f-cosine-[math.cos x]
var.f-root-[math.sqrt x]
```

### Comments
```blaze
// Single line comment
## Another style of comment
```

### Solid Numbers
```blaze
# Quick syntax
var.d-exact-["DEADBEEF!"]      # Exact solid (! = no terminals)
var.d-quantum-["3A7F~9B2C"]     # Quantum barrier (~ = with terminals)

# Full notation
var.d-pi-[3.14159...(q:10³⁵|0.85)...2653589]
#         known...(barrier:gap|confidence)...terminal

# Barriers: q=quantum, e=energy, s=storage, t=temporal, c=computational
```

For a complete explanation → [Solid Numbers Documentation](blaze/docs/SOLID_NUMBERS_IMPLEMENTATION.md)

## Examples

### Hello World
```blaze
print/ "Hello, World!" \
```

### Variables and Arithmetic
```blaze
var.v-x-[10]
var.v-y-[20]
var.v-sum-[x + y]
print/ sum \
```

### Functions
```blaze
|greet| func.can<
    print/ "Welcome to Blaze!" \
:>

^greet/
```

### Complex Example
```blaze
var.f-radius-[5.0]
var.f-pi-[3.14159]
var.f-area-[pi * radius * radius]

|show_area| func.can<
    print/ "Circle area: " \
    print/ area \
:>

^show_area/
```

### Solid Number Example
```blaze
# Calculate with uncertainty
var.d-measurement-[42.7...(q:10²|0.95)...8]
var.d-factor-[2.0...(exact)...]

|analyze| func.can<
    print/ "Measurement: " \
    print/ measurement \
:>

^analyze/
```

## Architecture

The Blaze compiler consists of several key components:

- **Lexer** (`src/lexer/`): Tokenizes Blaze source code
- **Parser** (`src/parser/`): Builds Abstract Syntax Tree (AST)
- **Code Generator** (`src/codegen/`): Generates x86-64 machine code
- **Symbol Table**: Manages variables and functions
- **Runtime** (`src/runtime/`): Minimal runtime support

## Advanced Features

### Temporal Flow Operators
- `</>` - Timeline divergence
- `>/>` - Timeline convergence
- `timeline-[name]` or `tl-[name]` - Create timeline
- `^timeline.[past/present/future]` - Temporal anchors
- Temporal memory zones with automatic GC migration

For details → [Time Travel Specification](blaze/docs/TIME_TRAVEL_SPEC.md)

### GGGX Optimization
The compiler includes the GGGX (Gap-Get-Glimpse-Guess) algorithm for computational feasibility analysis:
- **GO**: Initial state analysis
- **GET**: Resource acquisition
- **GAP**: Identify computational gaps
- **GLIMPSE**: Preview possible paths
- **GUESS**: Select optimal execution

For details → [GGGX Framework](blaze/docs/GGGX_FRAMEWORK.md)

### Scalable Code Generation
Supports generating executables larger than 1MB through a multi-segment architecture with automatic buffer management.

## Development

### Project Structure
```
blaze/
├── include/          # Header files
├── src/
│   ├── lexer/       # Lexical analysis
│   ├── parser/      # Syntax analysis
│   ├── codegen/     # Code generation
│   └── runtime/     # Runtime support
├── tests/           # Test files
├── docs/            # Documentation
└── Makefile         # Build configuration
```

### Contributing
See [CONTRIBUTING.md](blaze/CONTRIBUTING.md) for detailed guidelines on:
- Code style and conventions
- Development setup
- Testing requirements
- Pull request process

Quick start:
1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (see commit guidelines in blaze/CONTRIBUTING.md)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Error Reporting

The compiler integrates with Sentry for automatic error tracking. Errors are logged to:
- Local: `blaze_errors.log`
- Remote: Sentry dashboard (when built with `make debug`)

## Documentation

- [Language Specification](blaze/docs/LANGUAGE_SPEC.md) - Complete syntax reference
- [Solid Numbers Implementation](blaze/docs/SOLID_NUMBERS_IMPLEMENTATION.md) - In-depth explanation
- [GGGX Framework](blaze/docs/GGGX_FRAMEWORK.md) - Computational feasibility analysis
- [Time Travel Specification](blaze/docs/TIME_TRAVEL_SPEC.md) - Temporal debugging features
- [Architecture Overview](blaze/docs/ARCHITECTURE.md) - Compiler internals
- [Examples](blaze/examples/) - Code samples and tutorials

## Known Limitations

- Currently supports Linux/Unix systems only
- Windows support requires WSL
- -O2/-O3 optimizations cause crashes (use -O1 or no optimization)
- Some advanced features are still in development:
  - Solid number arithmetic operations
  - String manipulation functions
  - Full time-travel debugging implementation

## Community

- **Issues**: [Report bugs or request features](https://github.com/COMMENTERTHE9/Blaze/issues)
- **Discussions**: [Ask questions and share ideas](https://github.com/COMMENTERTHE9/Blaze/discussions)
- **Contributing**: [See how to help](blaze/CONTRIBUTING.md)

## License

This project is licensed under the MIT License - see the [LICENSE](blaze/LICENSE) file for details.

```
MIT License
Copyright (c) 2024 Gabriel I.T.
```

## Author

Gabriel I.T.

## Acknowledgments

Special thanks to the industrial computation community for inspiration on the GGGX algorithm and temporal flow concepts.