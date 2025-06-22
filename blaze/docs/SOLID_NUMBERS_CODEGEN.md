# Solid Numbers Code Generation Guide

## 1. Runtime Representation

### 1.1 Memory Layout

```c
// Runtime solid number structure (64 bytes aligned)
typedef struct {
    // Known digits (arbitrary precision)
    uint8_t* digits;      // 8 bytes - BCD encoded digits
    uint32_t digit_count; // 4 bytes - number of digits
    uint32_t decimal_pos; // 4 bytes - decimal point position
    
    // Barrier and gap
    uint8_t barrier_type; // 1 byte - 'q','e','s','t','c','∞','u','x'
    uint8_t _pad1[7];     // 7 bytes - padding for alignment
    uint64_t gap_magnitude; // 8 bytes - 10^n or UINT64_MAX
    
    // Confidence (fixed point)
    uint32_t confidence;  // 4 bytes - 0-1000000 (6 decimals)
    uint8_t _pad2[4];     // 4 bytes - padding
    
    // Terminal digits
    uint8_t terminal[8];  // 8 bytes - up to 8 terminal digits
    uint8_t terminal_len; // 1 byte - actual length
    uint8_t terminal_type;// 1 byte - 0=normal, 1=undef, 2=super
    uint8_t _pad3[6];     // 6 bytes - padding
} SolidNumber; // Total: 64 bytes
```

### 1.2 Stack Frame Layout

```asm
; SolidNumber on stack (RBP-relative)
; [RBP-8]   digits pointer
; [RBP-12]  digit_count
; [RBP-16]  decimal_pos
; [RBP-17]  barrier_type
; [RBP-24]  (padding)
; [RBP-32]  gap_magnitude
; [RBP-36]  confidence
; [RBP-40]  (padding)
; [RBP-48]  terminal[0-7]
; [RBP-49]  terminal_len
; [RBP-50]  terminal_type
; [RBP-56]  (padding)
```

## 2. Basic Operations Assembly

### 2.1 Loading Solid Number Literal

```asm
load_solid_literal:
    ; Input: RSI = AST node pointer
    ; Output: RAX = pointer to allocated SolidNumber
    
    ; Allocate SolidNumber (64 bytes)
    mov rdi, 64
    call malloc
    mov rbx, rax          ; Save pointer
    
    ; Initialize known digits
    mov rdi, [rsi+8]      ; known_offset in string pool
    mov rsi, [rsi+16]     ; known_len
    call parse_known_digits
    mov [rbx], rax        ; digits pointer
    mov [rbx+8], edx      ; digit_count
    mov [rbx+12], ecx     ; decimal_pos
    
    ; Set barrier info
    movzx eax, byte [rsi+24] ; barrier_type
    mov [rbx+16], al
    mov rax, [rsi+32]     ; gap_magnitude
    mov [rbx+24], rax
    
    ; Set confidence (convert from x1000 to x1000000)
    movzx eax, word [rsi+40] ; confidence_x1000
    imul eax, 1000
    mov [rbx+32], eax
    
    ; Copy terminal
    mov rdi, rbx
    add rdi, 40           ; &terminal[0]
    mov rsi, [rsi+48]     ; terminal string pool offset
    mov cl, [rsi+56]      ; terminal_len
    mov [rbx+48], cl
    mov dl, [rsi+57]      ; terminal_type
    mov [rbx+49], dl
    call copy_terminal
    
    mov rax, rbx
    ret
```

### 2.2 Solid Addition

```asm
solid_add:
    ; Input: RSI = SolidNumber* a, RDI = SolidNumber* b
    ; Output: RAX = SolidNumber* result
    
    push rbp
    mov rbp, rsp
    sub rsp, 80          ; Space for result + locals
    
    ; Allocate result
    mov rdi, 64
    call malloc
    mov [rbp-8], rax     ; Save result pointer
    
    ; 1. Add known digits (arbitrary precision)
    mov rdi, [rsi]       ; a->digits
    mov rsi, [rsi+8]     ; a->digit_count
    mov rdx, [rdi]       ; b->digits
    mov rcx, [rdi+8]     ; b->digit_count
    call add_arbitrary_precision
    mov rbx, [rbp-8]
    mov [rbx], rax       ; result->digits
    mov [rbx+8], edx     ; result->digit_count
    mov [rbx+12], ecx    ; result->decimal_pos
    
    ; 2. Select minimum barrier
    mov rsi, [rbp+16]    ; Restore a
    mov rdi, [rbp+24]    ; Restore b
    movzx eax, byte [rsi+16] ; a->barrier_type
    movzx ecx, byte [rdi+16] ; b->barrier_type
    call min_barrier_type
    mov [rbx+16], al
    
    ; 3. Select minimum gap
    mov rax, [rsi+24]    ; a->gap_magnitude
    mov rcx, [rdi+24]    ; b->gap_magnitude
    cmp rax, rcx
    cmova rax, rcx       ; min(a,b)
    mov [rbx+24], rax
    
    ; 4. Multiply confidence (fixed point)
    mov eax, [rsi+32]    ; a->confidence
    mov ecx, [rdi+32]    ; b->confidence
    ; Multiply and scale: (a * b) / 1000000
    mul ecx              ; EDX:EAX = a * b
    mov ecx, 1000000
    div ecx              ; EAX = result
    mov [rbx+32], eax
    
    ; 5. Add terminals (modular)
    lea rdi, [rbx+40]    ; result->terminal
    lea rsi, [rsi+40]    ; a->terminal
    lea rdx, [rdi+40]    ; b->terminal (rdi still has b)
    movzx ecx, byte [rsi+8]  ; a->terminal_len
    movzx r8d, byte [rsi+9]  ; a->terminal_type
    movzx r9d, byte [rdx+9]  ; b->terminal_type
    call add_terminals_modular
    
    mov rax, [rbp-8]     ; Return result
    leave
    ret
```

### 2.3 Terminal Arithmetic

```asm
add_terminals_modular:
    ; Input: RDI = result terminal ptr
    ;        RSI = a terminal ptr
    ;        RDX = b terminal ptr
    ;        RCX = terminal length
    ;        R8  = a terminal type
    ;        R9  = b terminal type
    
    ; Check for undefined
    cmp r8b, 1           ; TERMINAL_UNDEFINED
    je .set_undefined
    cmp r9b, 1
    je .set_undefined
    
    ; Check for superposition
    cmp r8b, 2           ; TERMINAL_SUPERPOSITION
    je .set_superposition
    cmp r9b, 2
    je .set_superposition
    
    ; Convert terminals to integers
    push rcx             ; Save length
    mov rdi, rsi
    movzx rsi, cl
    call terminal_to_int
    mov r10, rax         ; a value
    
    mov rdi, rdx
    pop rsi              ; Restore length
    push rsi
    call terminal_to_int
    mov r11, rax         ; b value
    
    ; Calculate modulus (10^length)
    mov rdi, 10
    pop rsi              ; length
    call pow_int
    mov rcx, rax         ; modulus
    
    ; Modular addition
    add r10, r11
    xor rdx, rdx
    mov rax, r10
    div rcx              ; RDX = remainder
    
    ; Convert back to terminal
    mov rdi, [rsp+8]     ; Original RDI (result ptr)
    mov rsi, rdx         ; Value
    mov rdx, rcx         ; Length
    call int_to_terminal
    
    mov byte [rdi+9], 0  ; TERMINAL_NORMAL
    ret
    
.set_undefined:
    mov byte [rdi+9], 1  ; TERMINAL_UNDEFINED
    mov byte [rdi], 0xE2 ; ∅ in UTF-8
    mov byte [rdi+1], 0x88
    mov byte [rdi+2], 0x85
    mov byte [rdi+8], 3  ; Length
    ret
    
.set_superposition:
    mov byte [rdi+9], 2  ; TERMINAL_SUPERPOSITION
    mov byte [rdi], '{'
    mov byte [rdi+1], '*'
    mov byte [rdi+2], '}'
    mov byte [rdi+8], 3
    ret
```

## 3. Division with Modular Inverse

### 3.1 Extended Euclidean Algorithm

```asm
extended_gcd:
    ; Input: RDI = a, RSI = m
    ; Output: RAX = gcd, RDX = inverse (or -1 if none)
    
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Initialize
    mov [rbp-8], rsi     ; old_r = m
    mov [rbp-16], rdi    ; r = a
    mov qword [rbp-24], 0 ; old_s = 0
    mov qword [rbp-32], 1 ; s = 1
    
.loop:
    cmp qword [rbp-16], 0
    je .done
    
    ; q = old_r / r
    mov rax, [rbp-8]
    xor rdx, rdx
    div qword [rbp-16]
    mov rcx, rax         ; q
    
    ; Update r
    mov rax, [rbp-16]    ; tmp = r
    mov rdx, rcx
    mul qword [rbp-16]
    mov rdx, [rbp-8]
    sub rdx, rax         ; old_r - q*r
    mov [rbp-8], rax     ; old_r = tmp
    mov [rbp-16], rdx    ; r = old_r - q*r
    
    ; Update s
    mov rax, [rbp-32]    ; tmp = s
    mov rdx, rcx
    mul qword [rbp-32]
    mov rdx, [rbp-24]
    sub rdx, rax         ; old_s - q*s
    mov [rbp-24], rax    ; old_s = tmp
    mov [rbp-32], rdx    ; s = old_s - q*s
    
    jmp .loop
    
.done:
    mov rax, [rbp-8]     ; gcd
    cmp rax, 1
    jne .no_inverse
    
    ; Normalize inverse to positive
    mov rdx, [rbp-24]    ; old_s
    test rdx, rdx
    jns .positive
    add rdx, rsi         ; Make positive
    
.positive:
    leave
    ret
    
.no_inverse:
    mov rdx, -1          ; No inverse exists
    leave
    ret
```

### 3.2 Solid Division

```asm
solid_divide:
    ; Similar structure to solid_add, but:
    
    ; ... (known digit division) ...
    
    ; 5. Divide terminals with modular inverse
    lea rdi, [rbx+40]    ; result->terminal
    lea rsi, [rsi+40]    ; a->terminal
    lea rdx, [rdi+40]    ; b->terminal
    movzx ecx, byte [rsi+8]  ; terminal_len
    
    ; Convert b terminal to integer
    push rsi
    push rdi
    mov rdi, rdx
    movzx rsi, cl
    call terminal_to_int
    mov r10, rax         ; b_term
    pop rdi
    pop rsi
    
    ; Calculate modulus
    push r10
    mov rdi, 10
    movzx rsi, cl
    call pow_int
    mov r11, rax         ; modulus
    pop r10
    
    ; Find modular inverse
    mov rdi, r10         ; b_term
    mov rsi, r11         ; modulus
    call extended_gcd
    cmp rdx, -1
    je .set_undefined_div
    
    ; Multiply a_term by inverse
    push rdx             ; Save inverse
    mov rdi, rsi
    movzx rsi, cl
    call terminal_to_int
    pop rdx
    mul rdx              ; a_term * inverse
    xor rdx, rdx
    div r11              ; mod modulus
    
    ; Convert result to terminal
    mov rsi, rdx
    mov rdx, rcx
    call int_to_terminal
    jmp .done_div
    
.set_undefined_div:
    mov byte [rdi+9], 1  ; TERMINAL_UNDEFINED
    ; Set ∅ symbol...
```

## 4. Special Number Handling

### 4.1 Infinity Arithmetic

```asm
handle_infinity_subtract:
    ; ∞ - ∞ = natural numbers
    
    ; Set known digits to "123456789101112..."
    mov rdi, natural_number_pattern
    call create_natural_sequence
    mov [rbx], rax       ; digits
    mov dword [rbx+8], -1 ; infinite digits
    
    mov byte [rbx+16], 0xFF ; barrier = ∞
    mov qword [rbx+24], -1  ; gap = ∞
    mov dword [rbx+32], 1000000 ; confidence = 1.0
    
    ; Terminal = {*}
    mov byte [rbx+40], '{'
    mov byte [rbx+41], '*'
    mov byte [rbx+42], '}'
    mov byte [rbx+48], 3
    mov byte [rbx+49], 2    ; TERMINAL_SUPERPOSITION
    ret

handle_infinity_divide:
    ; ∞ ÷ ∞ special algorithm
    
    ; Extract terminals (must be numeric for this algorithm)
    mov rdi, [rsi+40]    ; a->terminal
    movzx rsi, byte [rsi+48]
    call terminal_to_int
    mov r10, rax         ; t1
    
    mov rdi, [rdi+40]    ; b->terminal
    movzx rsi, byte [rdi+48]
    call terminal_to_int
    mov r11, rax         ; t2
    
    ; Calculate starting value
    mov rax, 12345678910
    xor rdx, rdx
    div r10              ; q1 = BASE/t1
    mov r12, rax
    
    mov rax, 12345678910
    xor rdx, rdx
    div r11              ; q2 = BASE/t2
    mov r13, rax
    
    ; q1/q2 (floating point)
    cvtsi2sd xmm0, r12
    cvtsi2sd xmm1, r13
    divsd xmm0, xmm1
    
    ; Convert to known digits
    ; ... (convert floating point to decimal string)
    
    ; Calculate terminal: (t1 * t2) mod 10^(k-1)
    mov rax, r10
    mul r11
    ; ... (modular reduction)
```

### 4.2 Exact Number Fast Path

```asm
check_exact_fast_path:
    ; Input: RSI, RDI = operands
    ; Output: ZF set if both exact
    
    cmp byte [rsi+16], 'x'  ; barrier_type == exact
    jne .not_exact
    cmp byte [rdi+16], 'x'
    jne .not_exact
    
    ; Both exact - use regular arithmetic
    call exact_arithmetic
    ret
    
.not_exact:
    xor eax, eax
    inc eax              ; Clear ZF
    ret
```

## 5. Optimization Strategies

### 5.1 SIMD Terminal Operations

```asm
simd_terminal_add:
    ; Add 4 terminals in parallel using SSE2
    
    movd xmm0, [rsi+40]  ; Load 4 bytes from a
    movd xmm1, [rdi+40]  ; Load 4 bytes from b
    
    ; Unpack bytes to words
    pxor xmm2, xmm2
    punpcklbw xmm0, xmm2
    punpcklbw xmm1, xmm2
    
    ; Add
    paddw xmm0, xmm1
    
    ; Modular reduction (simplified for demo)
    ; ... complex modular arithmetic in SIMD
    
    ; Pack back to bytes
    packuswb xmm0, xmm2
    movd [rbx+40], xmm0
    ret
```

### 5.2 Common Value Cache

```asm
section .data
cached_pi:    dq 0
cached_e:     dq 0
cached_sqrt2: dq 0

section .text
get_cached_solid:
    ; Input: RDI = constant ID (0=pi, 1=e, 2=sqrt2)
    ; Output: RAX = SolidNumber* (cached or computed)
    
    lea rsi, [cached_pi]
    mov rax, [rsi + rdi*8]
    test rax, rax
    jnz .cached
    
    ; Compute and cache
    push rdi
    call compute_solid_constant
    pop rdi
    lea rsi, [cached_pi]
    mov [rsi + rdi*8], rax
    
.cached:
    ret
```

## 6. Error Handling

### 6.1 Confidence Threshold Check

```asm
check_confidence_threshold:
    ; Input: RSI = SolidNumber*
    ; Output: Sets CF if below threshold
    
    mov eax, [rsi+32]    ; confidence
    cmp eax, 10000       ; 0.01 threshold
    jb .below_threshold
    clc
    ret
    
.below_threshold:
    stc
    ret
```

### 6.2 Barrier Warnings

```asm
warn_barrier_mismatch:
    ; When operations mix different barriers
    
    push rsi
    push rdi
    
    ; Format warning message
    mov rdi, barrier_warning_fmt
    movzx rsi, byte [rsi+16]  ; barrier1
    movzx rdx, byte [rdi+16]  ; barrier2
    call format_warning
    
    ; Log to error system
    mov rdi, rax
    call log_warning
    
    pop rdi
    pop rsi
    ret
```

## 7. Integration with Blaze

### 7.1 Type System Integration

```c
// In type checking
bool is_solid_type(uint16_t node_idx) {
    return nodes[node_idx].type == NODE_SOLID;
}

// Type promotion rules
uint16_t promote_to_solid(uint16_t regular_node) {
    // Convert regular number to exact solid
    uint16_t solid = alloc_node(NODE_SOLID);
    // ... set exact barrier, confidence 1.0, etc.
    return solid;
}
```

### 7.2 Mixed Operations

```asm
handle_mixed_operation:
    ; When solid + regular number
    
    ; Check if first operand is solid
    cmp byte [rsi], NODE_SOLID
    je .first_solid
    
    ; Second must be solid, promote first
    call promote_regular_to_solid
    xchg rsi, rax
    jmp .both_solid
    
.first_solid:
    ; Check second
    cmp byte [rdi], NODE_SOLID
    je .both_solid
    
    ; Promote second
    call promote_regular_to_solid
    mov rdi, rax
    
.both_solid:
    ; Now perform solid operation
    call solid_operation
    ret
```

## 8. Debug Support

### 8.1 Solid Number Printing

```asm
print_solid_number:
    ; Pretty-print solid number
    
    ; Print known digits
    mov rdi, [rsi]       ; digits
    mov rsi, [rsi+8]     ; count
    call print_decimal
    
    ; Print "..."
    mov rdi, ellipsis_str
    call print_str
    
    ; Print "("
    mov rdi, lparen_str
    call print_str
    
    ; Print barrier type
    movzx edi, byte [rsi+16]
    call print_char
    
    ; ... continue formatting
```

### 8.2 Trace Macros

```asm
%macro TRACE_SOLID_OP 2
    %ifdef DEBUG
    push rsi
    push rdi
    mov rdi, solid_trace_fmt
    mov rsi, %1          ; operation name
    mov rdx, %2          ; extra info
    call debug_trace
    pop rdi
    pop rsi
    %endif
%endmacro

; Usage:
solid_multiply:
    TRACE_SOLID_OP "multiply", rsi
    ; ... operation code
```

## 9. Performance Considerations

1. **Arbitrary Precision**: Use GMP or custom BCD for known digits
2. **Terminal Operations**: Keep in registers when possible
3. **Confidence Tracking**: Use integer arithmetic (scale by 10^6)
4. **Memory Pool**: Pre-allocate SolidNumber structs
5. **Fast Paths**: Detect exact numbers, common operations
6. **SIMD**: Use for parallel terminal arithmetic

## 10. Testing Infrastructure

```asm
; Test helper macros
%macro TEST_SOLID_ADD 4
    ; %1,%2 = input terminals
    ; %3 = expected terminal
    ; %4 = test name
    
    push rbp
    mov rbp, rsp
    sub rsp, 128
    
    ; Create test solids
    lea rsi, [rbp-64]
    mov byte [rsi+40], %1  ; terminal digit
    lea rdi, [rbp-128]
    mov byte [rdi+40], %2
    
    call solid_add
    
    ; Check result
    movzx eax, byte [rax+40]
    cmp al, %3
    je %%pass
    
    ; Fail
    mov rdi, test_fail_fmt
    mov rsi, %4
    call test_fail
    jmp %%done
    
%%pass:
    mov rdi, test_pass_fmt
    mov rsi, %4
    call test_pass
    
%%done:
    leave
%endmacro
```