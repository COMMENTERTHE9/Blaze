# Blaze v0.1.0 Release Notes

## 🔥 First Official Release!

This is the first official release of the Blaze programming language compiler.

### What's Working

✅ **Core Language Features**
- Variables (int, float, string, bool, solid numbers)
- Functions with the unique `|name| verb.can< ... :>` syntax
- Print statements with proper `print/ "text" \` syntax
- Basic arithmetic operators (+, -, *, /, %, **)
- Bitwise operators (&&., ||., ^^, ~~, <<., >>.)
- Math functions (sin, cos, tan, sqrt, log, exp, floor, ceil, round)

✅ **Revolutionary Features**
- **Solid Numbers**: Numbers with computational barriers
- **Direct x86-64 compilation**: No runtime, no dependencies
- **Water flow operators**: `<` and `>` for natural flow control
- **Time-aware syntax**: Built for temporal computation

### Installation

1. Download the appropriate release for your platform
2. Extract the archive:
   ```bash
   tar -xzf blaze-v0.1.0-linux-x86_64.tar.gz
   cd blaze-v0.1.0-linux-x86_64
   ```
3. Run Blaze:
   ```bash
   ./bin/blaze examples/hello_world.blaze output
   ./output
   ```

### Example Program

```blaze
# Simple Blaze program
var.v-x-[42]
var.v-y-[math.sqrt(16)]

print/ "The answer is: " \
print/ x \
print/ " and sqrt(16) = " \
print/ y \
```

### Known Limitations

- String variables store only memory addresses (fix coming soon)
- Float arithmetic still being integrated
- Some advanced temporal features not yet implemented
- Windows/macOS builds require GitHub Actions

### What's Next

- Temporal memory zones (PAST, PRESENT, FUTURE)
- The Forge build tool for Blaze projects
- More math functions and operators
- Complete float support

### Contributing

See [CONTRIBUTING.md](https://github.com/gabrieldechichi/programming-studies/blob/main/programming-languages/blaze/CONTRIBUTING.md) for guidelines.

### License

Blaze is released under the MIT License.