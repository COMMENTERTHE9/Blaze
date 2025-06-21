# Blaze Compiler

A revolutionary programming language compiler that translates Blaze language to x86-64 machine code, featuring time-travel debugging, GGGX optimization, and industrial-strength computation support.

## Features

- **Direct Machine Code Generation**: Compiles directly to x86-64 assembly without intermediate representations
- **Time-Travel Debugging**: Built-in temporal flow operators for advanced debugging capabilities
- **GGGX Algorithm**: Gap-aware optimization for industrial computation
- **Scalable Code Generation**: Supports output files >1MB with multi-segment architecture
- **Sentry Integration**: Automatic error tracking and reporting
- **SSE/Float Support**: Full floating-point arithmetic with SSE instructions

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
var.v-name-[value]
var.v-x-[10]
var.v-y-[x + 5]
var.v-pi-[3.14159]
```

### Functions
```blaze
|function_name| func.can<
    var.v-local-[42]
    print/ local\
:>

^function_name/
```

### Print Statements
```blaze
print/ "Hello, World!"\
print/ variable_name\
print/ x + y\
```

### Arithmetic Operations
```blaze
var.v-sum-[a + b]
var.v-diff-[a - b]
var.v-prod-[a * b]
var.v-quot-[a / b]
```

### Comments
```blaze
// Single line comment
```

## Examples

### Hello World
```blaze
print/ "Hello, World!"\
```

### Variables and Arithmetic
```blaze
var.v-x-[10]
var.v-y-[20]
var.v-sum-[x + y]
print/ sum\
```

### Functions
```blaze
|greet| func.can<
    print/ "Welcome to Blaze!"\
:>

^greet/
```

### Complex Example
```blaze
var.v-radius-[5.0]
var.v-pi-[3.14159]
var.v-area-[pi * radius * radius]

|show_area| func.can<
    print/ "Circle area: "\
    print/ area\
:>

^show_area/
```

## Architecture

The Blaze compiler consists of several key components:

- **Lexer** (`src/lexer/`): Tokenizes Blaze source code
- **Parser** (`src/parser/`): Builds Abstract Syntax Tree (AST)
- **Code Generator** (`src/codegen/`): Generates x86-64 machine code
- **Symbol Table**: Manages variables and functions
- **Runtime** (`src/runtime/`): Minimal runtime support

## Advanced Features

### Temporal Flow Operators (Planned)
- `</>` - Timeline divergence
- `>/>` - Timeline convergence
- `timeline.[past/present/future]` - Temporal anchors

### GGGX Optimization
The compiler includes provisional support for GGGX (Gap-aware) optimization, which analyzes code patterns to predict optimal execution paths.

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
1. Fork the repository
2. Create your feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

## Error Reporting

The compiler integrates with Sentry for automatic error tracking. Errors are logged to:
- Local: `blaze_errors.log`
- Remote: Sentry dashboard (when built with `make debug`)

## Known Limitations

- Currently supports Linux/Unix systems only
- Windows support requires WSL
- Some advanced features are still in development

## License

This project is under active development. License details coming soon.

## Author

Gabriel I.T.

## Acknowledgments

Special thanks to the industrial computation community for inspiration on the GGGX algorithm and temporal flow concepts.