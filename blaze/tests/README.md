# Blaze Test Organization

## Directory Structure

### `control_flow/`
Tests for control flow features:
- **While loops**: `test_while_*.blaze`, `test_*_while.blaze`
- **For loops**: `test_for_*.blaze`, `test_*_for.blaze`
- **Conditionals**: `test_conditional.blaze`, `test_*_condition*.blaze`
- **Loop variations**: counting, simple, debug, fixed versions

### `data_types/`
Tests for data types and variables:
- **Variable operations**: `test_var*.blaze`
- **Arithmetic**: `test_arithmetic.blaze`, `test_*_ops.blaze`
- **Type system**: `test_type_system_features.blaze`
- **Comparisons**: `test_*comparison*.blaze`
- **Float operations**: `test_float.blaze`
- **Constants/typedef**: `test_const_typedef.blaze`

### `output/`
Tests for output and printing functionality:
- **Print operations**: `test_*print*.blaze`
- **Output methods**: various output format tests

### `syntax/`
Tests for general syntax features:
- **Syntax variations**: `test_both_syntaxes.blaze`
- **General syntax**: `test_simple*.blaze`

### `regression/`
Debug tests, bug fixes, and edge cases:
- **Debug tests**: `test_debug*.blaze`
- **Output variations**: `test_output*.blaze`
- **Binary outputs**: `*.bin` files
- **Scripts**: `test_timeout.sh`
- **Bug fixes**: various fixed test versions
- **GGGX tests**: `test_gggx*`

### `working/` (existing)
Stable, numbered test suite with examples and demos

### `old/` (existing)  
Legacy test files and C test implementations

### `broken/` (existing)
Non-functional tests requiring fixes

## Test File Naming Convention

- **Source files**: `test_*.blaze`
- **Output files**: `test_*_output` (expected output)
- **Binary outputs**: `test_*.bin`
- **Scripts**: `test_*.sh`

## Running Tests

Individual test categories can be run by navigating to the specific directory:
```bash
cd blaze/tests/control_flow
# Run specific loop tests
cd blaze/tests/data_types  
# Run variable and type tests
```

For comprehensive testing, run tests from the main working directory which contains the most stable test suite.