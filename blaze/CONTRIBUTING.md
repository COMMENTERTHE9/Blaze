# Contributing to Blaze 🔥

First off, thank you for considering contributing to Blaze! Every contribution helps make Blaze better, whether it's fixing a typo, adding a feature, or improving documentation. We're excited to have you join our community.

## Table of Contents
- [Ways to Contribute](#ways-to-contribute)
- [Getting Started](#getting-started)
- [Development Process](#development-process)
- [Code Style Guidelines](#code-style-guidelines)
- [Commit Guidelines](#commit-guidelines)
- [Pull Request Process](#pull-request-process)
- [Testing](#testing)
- [Documentation](#documentation)
- [Getting Help](#getting-help)

## Ways to Contribute

There are many ways to contribute to Blaze:

- **Report bugs** - Found something broken? Let us know!
- **Suggest features** - Have an idea for making Blaze better?
- **Improve documentation** - Help others understand Blaze
- **Fix bugs** - Pick an issue and dive in
- **Add new features** - Implement something amazing
- **Write tests** - Help us maintain quality
- **Create examples** - Show others what's possible

No contribution is too small! Even fixing a typo helps.

## Getting Started

### Prerequisites

- GCC or compatible C compiler
- Make
- Linux/Unix environment (Windows users: WSL works great!)
- Git

### Setting Up Your Development Environment

1. **Fork the repository** on GitHub

2. **Clone your fork**:
   ```bash
   git clone https://github.com/YOUR_USERNAME/Blaze.git
   cd Blaze
   ```

3. **Add upstream remote**:
   ```bash
   git remote add upstream https://github.com/COMMENTERTHE9/Blaze.git
   ```

4. **Build the project**:
   ```bash
   make clean
   make debug  # For development with debug symbols
   ```

5. **Run a test**:
   ```bash
   ./blaze_debug test_basic_var.blaze output
   ./output
   ```

### Project Structure

```
blaze/
├── include/          # Header files
├── src/
│   ├── lexer/       # Tokenization
│   ├── parser/      # AST generation
│   ├── codegen/     # x86-64 code generation
│   └── runtime/     # Runtime support (memory, GC)
├── tests/           # Test suite
├── docs/            # Documentation
├── examples/        # Example Blaze programs
└── stdlib/          # Standard library (WIP)
```

## Development Process

### 1. Start with an Issue

- Check existing issues or create a new one
- Comment on the issue to claim it
- Wait for feedback before starting major work

### 2. Create a Feature Branch

```bash
git checkout -b feature/your-feature-name
# or
git checkout -b fix/issue-number-description
```

### 3. Make Your Changes

- Write clean, readable code
- Follow existing patterns in the codebase
- Add tests for new functionality
- Update documentation as needed

### 4. Test Your Changes

```bash
# Run existing tests
./test.sh

# Test your specific changes
./blaze_debug your_test.blaze output && ./output
```

### 5. Commit Your Changes

See [Commit Guidelines](#commit-guidelines) below.

## Code Style Guidelines

### Blaze Language Philosophy

Blaze embraces unique design choices:

- **No parentheses** - We use punctuation for syntax, not grouping
- **Forward slash operations** - `print/ "Hello" \` not `print("Hello")`
- **Closing backslashes** - All output statements need `\` to terminate
- **Punctuation-based** - `.`, `/`, `\`, `-`, `[`, `]` have meaning

### C Code Style

```c
// Good: Clear, simple, direct
void generate_var_store(CodeBuffer* buf, const char* var_name, X64Register reg) {
    VarEntry* var = get_or_create_var(var_name);
    if (!var) return;
    
    emit_mov_mem_reg(buf, RSP, 256 + var->stack_offset, reg);
    var->is_initialized = true;
}

// Bad: Over-complicated
void generate_var_store(CodeBuffer* buf, const char* var_name, X64Register reg) {
    VarEntry* var = NULL;
    if ((var = get_or_create_var(var_name)) != NULL) {
        emit_mov_mem_reg(buf, RSP, 256 + var->stack_offset, reg);
        var->is_initialized = true;
    }
}
```

### Naming Conventions

- Functions: `snake_case` - `generate_expression()`, `emit_mov_reg_imm64()`
- Types: `PascalCase` - `CodeBuffer`, `ASTNode`, `VarEntry`
- Constants: `UPPER_SNAKE_CASE` - `MAX_TOKENS`, `NODE_NUMBER`
- Enums: `UPPER_SNAKE_CASE` - `TOK_PRINT`, `NODE_VAR_DEF`

### Error Handling

```c
// Always check for errors
if (!buffer || !nodes || node_idx >= MAX_NODES) {
    print_str("[ERROR] Invalid parameters\n");
    return;
}

// Provide helpful error messages
if (var_count >= MAX_VARS) {
    print_str("[ERROR] Too many variables (max ");
    print_num(MAX_VARS);
    print_str(")\n");
    return NULL;
}
```

## Commit Guidelines

### Format

```
type: short description

Longer explanation if needed. Wrap at 72 characters.

- Bullet points for multiple changes
- Keep it factual and clear
```

### Types

- `feat:` New feature
- `fix:` Bug fix
- `docs:` Documentation only
- `style:` Formatting, missing semicolons, etc
- `refactor:` Code change that neither fixes a bug nor adds a feature
- `test:` Adding missing tests
- `chore:` Maintenance tasks

### Examples

```bash
# Good
git commit -m "feat: add string variable support"
git commit -m "fix: correct stack alignment in function calls"
git commit -m "docs: update README with solid number examples"

# Bad
git commit -m "Updated stuff"
git commit -m "fix"
git commit -m "WIP"
```

### Important: No AI Attribution

This is a human-maintained project. Please:
- Write your own commit messages
- Don't include AI tool references
- Take credit for your work!

## Pull Request Process

### 1. Before Creating a PR

- Ensure all tests pass
- Update documentation if needed
- Rebase on latest main:
  ```bash
  git fetch upstream
  git rebase upstream/main
  ```

### 2. Creating the PR

Use this template:

```markdown
## Summary
Brief description of changes

## Related Issue
Fixes #123

## Changes Made
- Added X functionality
- Fixed Y bug
- Updated Z documentation

## Testing
- [ ] All existing tests pass
- [ ] Added new tests for changes
- [ ] Manually tested with example: [provide example]

## Screenshots (if applicable)
```

### 3. Review Process

- Maintainers will review within 3-5 days
- Address feedback promptly
- Be patient and respectful
- Celebrate when merged! 🎉

## Testing

### Running Tests

```bash
# Run all tests
./test.sh

# Run specific test
./blaze_debug test_name.blaze output && ./output
```

### Writing Tests

Create test files following the pattern:

```blaze
## test_feature_name.blaze
var.v-x-[10]
var.v-y-[20]
var.v-sum-[x + y]
print/ sum \
```

Test files should:
- Focus on one feature
- Be minimal but complete
- Include comments explaining what's tested
- Have descriptive names

### Coverage Expectations

- New features need tests
- Bug fixes need regression tests
- Aim for demonstrating the feature works

## Documentation

### When to Update Docs

Update docs when you:
- Add new features
- Change syntax
- Fix confusing explanations
- Add examples

### Documentation Locations

- `README.md` - User-facing overview
- `docs/` - Detailed specifications
- `CLAUDE.md` - Implementation notes
- Code comments - Implementation details

### Documentation Style

- Be clear and concise
- Use examples liberally
- Explain the "why" not just "what"
- Keep technical docs technical

## Getting Help

### Where to Ask Questions

- **GitHub Issues** - For bugs and features
- **Discussions** - For questions and ideas
- **Pull Request** - For code review feedback

### Finding Good First Issues

Look for issues labeled:
- `good first issue`
- `help wanted`
- `documentation`

### Tips for New Contributors

1. Start small - pick a simple issue first
2. Read existing code before writing new code
3. Ask questions - we're here to help!
4. Don't be afraid to make mistakes
5. Have fun with it!

## Code of Conduct

Be respectful, inclusive, and constructive. We're all here to make Blaze better together.

---

**Remember**: You're not just contributing code, you're joining a community. Welcome aboard! 🚀

If you use AI tools to help you code, that's totally fine! Just make sure you understand what you're submitting and write your own commit messages.