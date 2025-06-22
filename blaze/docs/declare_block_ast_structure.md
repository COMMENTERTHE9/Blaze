# Declare Block AST Structure Analysis

## How Declare Blocks Work

Looking at the parser code, here's what happens:

1. **No NODE_DECLARE_BLOCK is created!** 
   - When `declare/` is encountered, the parser sets a global flag `in_declare_block = true`
   - It returns `0xFFFE` which is skipped by the main parser
   - The backslash `\` at the end clears the flag and also returns `0xFFFE`

2. **Functions inside declare blocks are direct children of NODE_PROGRAM**
   - They are parsed normally by `parse_pipe_func_def()`
   - They are marked with `temporal_offset = 1` to indicate they're declared
   - They are chained with other statements at the program level

## AST Structure for Your Example

```
declare/
|test| entry.can< do/
  print/ "F"
:>
\

do/
  print/ "1"
\
```

The AST looks like this:

```
NODE_PROGRAM (root)
├── left_idx: NODE_FUNC_DEF (|test|)
│   ├── type: NODE_FUNC_DEF
│   ├── temporal_offset: 0x00010001 (lower 16 bits = 1, upper 16 bits = name_node)
│   ├── body (stored elsewhere): NODE_ACTION_BLOCK
│   │   └── NODE_OUTPUT (print/ "F")
│   └── (next via binary.right_idx)
│
└── binary.right_idx: NODE_ACTION_BLOCK (do/)
    └── NODE_OUTPUT (print/ "1")
```

## Key Points:

1. **There is NO NODE_DECLARE_BLOCK in the AST!**
   - The declare block is handled purely through parser state
   - It's just a way to mark functions with a flag

2. **Functions are differentiated by a flag**
   - Functions inside declare blocks: `temporal_offset & 0xFFFF == 1`
   - Functions outside declare blocks: `temporal_offset & 0xFFFF == 0`

3. **All statements are siblings**
   - The function definition and the main `do/` block are siblings
   - They're connected via the `binary.right_idx` chain
   - No hierarchical nesting for declare blocks

## Why This Design?

This flat structure makes sense because:
- Declare blocks are just a syntactic grouping for forward declarations
- Functions need to be accessible globally regardless of where they're declared
- The compiler can easily separate declared functions by checking the flag
- No need for extra traversal through a DECLARE_BLOCK node

## Compiler Processing

The compiler likely:
1. First pass: Collects all nodes with `NODE_FUNC_DEF` and `temporal_offset & 0xFFFF == 1`
2. Generates code for these declared functions
3. Second pass: Processes remaining statements

This explains why your function might not be executing - the compiler needs to properly handle both declared and non-declared functions!