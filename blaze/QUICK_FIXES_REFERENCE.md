# Quick Fixes Reference - Blaze Compiler

## 🚨 If Variables Aren't Working
**Symptom**: Segfault or variables not storing/retrieving
**Fix**: Check syntax is `var.v-name-[value]` NOT `v/ x 5`

## 🚨 If Functions Print Wrong Values
**Symptom**: Functions always print "5" or wrong values
**Location**: `src/codegen/codegen_func.c`
**Fix**: Function names are in upper 16 bits of temporal_offset
```c
uint16_t name_offset = (func_node->data.timing.temporal_offset >> 16) & 0xFFFF;
```

## 🚨 If "Unsupported print type: 24"
**Symptom**: Error when printing inside functions
**Location**: `src/codegen/codegen_output.c`
**Fix**: Check if content_idx > 10000 (string literal vs node)

## 🚨 If Type 243 Error
**Symptom**: Random crashes, AST corruption
**Action**: Check Sentry dashboard, errors auto-reported

## 🚨 If No Errors in Sentry
**Fix**: Use `./blaze_debug` NOT `./blaze_compiler`
**Build**: `make debug` to get Sentry support

## Working Examples

### Variables
```blaze
var.v-x-[10]
var.v-y-[20]
print/ x
print/ y
```

### Functions
```blaze
|myfunc| logic.can<
    print/ 42
:>
^myfunc/
```

### Combined
```blaze
var.v-a-[5]
|func| entry.can<
    print/ a
:>
do/
    ^func/
\
```