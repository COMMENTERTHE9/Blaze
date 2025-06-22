// Math function code generation for Blaze
// Implements sin, cos, tan, sqrt, log, exp, etc.

#include "blaze_internals.h"
#include <math.h>

// Forward declarations
extern void emit_mov_reg_imm64(CodeBuffer* buf, X64Register reg, uint64_t value);
extern void emit_mov_reg_reg(CodeBuffer* buf, X64Register dst, X64Register src);
extern void emit_push_reg(CodeBuffer* buf, X64Register reg);
extern void emit_pop_reg(CodeBuffer* buf, X64Register reg);
extern void emit_call_reg(CodeBuffer* buf, X64Register reg);
extern void emit_sub_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_add_reg_imm32(CodeBuffer* buf, X64Register reg, int32_t value);
extern void emit_movsd_xmm_mem(CodeBuffer* buf, SSERegister dst, X64Register base);
extern void emit_movsd_mem_xmm(CodeBuffer* buf, X64Register base, SSERegister src);
extern void emit_cvtsi2sd_xmm_reg(CodeBuffer* buf, SSERegister dst, X64Register src);
extern void emit_cvtsd2si_reg_xmm(CodeBuffer* buf, X64Register dst, SSERegister src);

// Math function type enumeration
typedef enum {
    MATH_SIN,
    MATH_COS,
    MATH_TAN,
    MATH_SQRT,
    MATH_LOG,
    MATH_EXP,
    MATH_ABS,
    MATH_FLOOR,
    MATH_CEIL,
    MATH_ROUND,
    MATH_POW,
    MATH_ATAN2
} MathFunctionType;

// Determine which math function from the name
static MathFunctionType get_math_function_type(const char* name, uint16_t len) {
    if (len == 3) {
        if (name[0] == 's' && name[1] == 'i' && name[2] == 'n') return MATH_SIN;
        if (name[0] == 'c' && name[1] == 'o' && name[2] == 's') return MATH_COS;
        if (name[0] == 't' && name[1] == 'a' && name[2] == 'n') return MATH_TAN;
        if (name[0] == 'a' && name[1] == 'b' && name[2] == 's') return MATH_ABS;
        if (name[0] == 'l' && name[1] == 'o' && name[2] == 'g') return MATH_LOG;
        if (name[0] == 'e' && name[1] == 'x' && name[2] == 'p') return MATH_EXP;
        if (name[0] == 'p' && name[1] == 'o' && name[2] == 'w') return MATH_POW;
    } else if (len == 4) {
        if (name[0] == 's' && name[1] == 'q' && name[2] == 'r' && name[3] == 't') return MATH_SQRT;
        if (name[0] == 'c' && name[1] == 'e' && name[2] == 'i' && name[3] == 'l') return MATH_CEIL;
    } else if (len == 5) {
        if (name[0] == 'f' && name[1] == 'l' && name[2] == 'o' && name[3] == 'o' && name[4] == 'r') return MATH_FLOOR;
        if (name[0] == 'r' && name[1] == 'o' && name[2] == 'u' && name[3] == 'n' && name[4] == 'd') return MATH_ROUND;
        if (name[0] == 'a' && name[1] == 't' && name[2] == 'a' && name[3] == 'n' && name[4] == '2') return MATH_ATAN2;
    }
    
    // Unknown function - this should be caught by parser
    return MATH_SIN; // Default
}

// Generate inline approximation for sin(x)
// Using Taylor series: sin(x) = x - x³/3! + x⁵/5! - x⁷/7! + ...
static void generate_sin_approximation(CodeBuffer* buf) {
    // Input in XMM0, output in XMM0
    // We'll use a simple approximation for now
    // Later can be improved with better algorithms
    
    // For now, just preserve the input
    // TODO: Implement actual sin calculation
}

// Generate inline approximation for cos(x)
// Using Taylor series: cos(x) = 1 - x²/2! + x⁴/4! - x⁶/6! + ...
static void generate_cos_approximation(CodeBuffer* buf) {
    // Input in XMM0, output in XMM0
    // For now, just preserve the input
    // TODO: Implement actual cos calculation
}

// Generate inline approximation for sqrt(x)
// x86-64 has a native SQRTSD instruction!
static void generate_sqrt(CodeBuffer* buf) {
    // Input in XMM0, output in XMM0
    // SQRTSD xmm0, xmm0
    emit_byte(buf, 0xF2);  // F2 prefix for scalar double
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x51);
    emit_byte(buf, 0xC0);  // ModRM: xmm0, xmm0
}

// Generate code for a math function call
void generate_math_function(CodeBuffer* buf, const char* func_name, uint16_t name_len,
                           ASTNode* nodes, uint16_t arg_idx,
                           SymbolTable* symbols, char* string_pool) {
    
    print_str("[MATH] Generating math function: ");
    for (uint16_t i = 0; i < name_len; i++) {
        char c[2] = {func_name[i], 0};
        print_str(c);
    }
    print_str("\n");
    
    // Determine which function
    MathFunctionType func_type = get_math_function_type(func_name, name_len);
    
    // Generate code to evaluate the argument
    // The argument expression should leave result in RAX (integer) or XMM0 (float)
    void generate_expression(CodeBuffer* buf, ASTNode* nodes, uint16_t expr_idx,
                            SymbolTable* symbols, char* string_pool);
    
    // Evaluate the argument
    generate_expression(buf, nodes, arg_idx, symbols, string_pool);
    
    // Check if we need to convert integer to float
    // For now, assume the argument is in RAX and needs conversion
    // Convert RAX to XMM0
    emit_cvtsi2sd_xmm_reg(buf, XMM0, RAX);
    
    // Generate the appropriate math function
    switch (func_type) {
        case MATH_SIN:
            generate_sin_approximation(buf);
            break;
            
        case MATH_COS:
            generate_cos_approximation(buf);
            break;
            
        case MATH_TAN:
            // tan(x) = sin(x) / cos(x)
            // For now, just preserve input
            break;
            
        case MATH_SQRT:
            generate_sqrt(buf);
            break;
            
        case MATH_ABS:
            // Absolute value - clear sign bit
            // ANDPD xmm0, [abs_mask] where abs_mask = 0x7FFFFFFFFFFFFFFF
            // For now, simplified implementation
            break;
            
        case MATH_LOG:
        case MATH_EXP:
        case MATH_FLOOR:
        case MATH_CEIL:
        case MATH_ROUND:
            // TODO: Implement these functions
            break;
            
        case MATH_POW:
        case MATH_ATAN2:
            // These take two arguments - need special handling
            break;
    }
    
    // Convert result back to integer for now
    // Later we'll support float results properly
    emit_cvtsd2si_reg_xmm(buf, RAX, XMM0);
}

// Check if a function name is a math function
bool is_math_function(const char* name, uint16_t len) {
    // Check common math function names
    if (len == 3) {
        if ((name[0] == 's' && name[1] == 'i' && name[2] == 'n') ||
            (name[0] == 'c' && name[1] == 'o' && name[2] == 's') ||
            (name[0] == 't' && name[1] == 'a' && name[2] == 'n') ||
            (name[0] == 'a' && name[1] == 'b' && name[2] == 's') ||
            (name[0] == 'l' && name[1] == 'o' && name[2] == 'g') ||
            (name[0] == 'e' && name[1] == 'x' && name[2] == 'p') ||
            (name[0] == 'p' && name[1] == 'o' && name[2] == 'w')) {
            return true;
        }
    } else if (len == 4) {
        if ((name[0] == 's' && name[1] == 'q' && name[2] == 'r' && name[3] == 't') ||
            (name[0] == 'c' && name[1] == 'e' && name[2] == 'i' && name[3] == 'l')) {
            return true;
        }
    } else if (len == 5) {
        if ((name[0] == 'f' && name[1] == 'l' && name[2] == 'o' && name[3] == 'o' && name[4] == 'r') ||
            (name[0] == 'r' && name[1] == 'o' && name[2] == 'u' && name[3] == 'n' && name[4] == 'd') ||
            (name[0] == 'a' && name[1] == 't' && name[2] == 'a' && name[3] == 'n' && name[4] == '2')) {
            return true;
        }
    }
    return false;
}