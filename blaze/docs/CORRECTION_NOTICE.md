# IMPORTANT CORRECTION

The initial documentation had incorrect information about flow operators.

## What Was Wrong

I incorrectly stated that:
- `/` indicates flow direction ❌ WRONG
- `^` shows data elevation/flow ❌ WRONG

## The Correct Understanding

### Separators (NOT flow)
- `/` - Command separator: `print/ "hello"` (like a colon)
- `\` - Statement terminator: `print/ "hello" \`
- `^` - Function invocation: `^function/` or timeline jump: `^timeline.[point]/`

### Real Flow Operators
- `>` - Forward flow: `data > variable`
- `<` - Backward flow: `variable < data`
- `>>` - Pipeline forward: `input >> process >> output`
- `<<` - Pipeline backward: `output << process << input`
- `</<` - Bidirectional flow: `a </< b`
- `*>` - Conditional branching: `*>condition> true_path/ false_path<`

## River Model

Blaze execution flows like a river:
- Can flow forward (downstream) with `>` `>>`
- Can flow backward (upstream) with `<` `<<`
- Can split at branches with `*>condition>`
- Can even flow back in time with `^timeline.[checkpoint]/`

This is fundamentally different from traditional "railroad track" execution!