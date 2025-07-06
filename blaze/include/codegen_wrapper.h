// CODE GENERATION WRAPPER
// Provides compatibility layer between existing CodeBuffer and new ScalableContext

#ifndef CODEGEN_WRAPPER_H
#define CODEGEN_WRAPPER_H

#include "blaze_internals.h"
#include "scalable_codegen.h"

// Global scalable context (initialized in main)
extern ScalableContext* g_scalable_ctx;

// Initialize global scalable context
void codegen_init_scalable(uint32_t initial_size);

// Wrapper functions that redirect to scalable implementation
static inline void emit_byte_scalable(CodeBuffer* buf, uint8_t byte) {
    if (g_scalable_ctx != NULL) {
        scalable_emit_byte(g_scalable_ctx, byte);
    } else {
        // Fallback to original implementation
        emit_byte(buf, byte);
    }
}

static inline void emit_word_scalable(CodeBuffer* buf, uint16_t word) {
    if (g_scalable_ctx != NULL) {
        scalable_emit_word(g_scalable_ctx, word);
    } else {
        emit_word(buf, word);
    }
}

static inline void emit_dword_scalable(CodeBuffer* buf, uint32_t dword) {
    if (g_scalable_ctx != NULL) {
        scalable_emit_dword(g_scalable_ctx, dword);
    } else {
        emit_dword(buf, dword);
    }
}

static inline void emit_qword_scalable(CodeBuffer* buf, uint64_t qword) {
    if (g_scalable_ctx != NULL) {
        scalable_emit_qword(g_scalable_ctx, qword);
    } else {
        emit_qword(buf, qword);
    }
}

// Macros to enable scalable code generation
#ifdef USE_SCALABLE_CODEGEN
    #define EMIT_BYTE(buf, byte) emit_byte_scalable(buf, byte)
    #define EMIT_WORD(buf, word) emit_word_scalable(buf, word)
    #define EMIT_DWORD(buf, dword) emit_dword_scalable(buf, dword)
    #define EMIT_QWORD(buf, qword) emit_qword_scalable(buf, qword)
#else
    #define EMIT_BYTE(buf, byte) emit_byte(buf, byte)
    #define EMIT_WORD(buf, word) emit_word(buf, word)
    #define EMIT_DWORD(buf, dword) emit_dword(buf, dword)
    #define EMIT_QWORD(buf, qword) emit_qword(buf, qword)
#endif

// Get total generated size
uint64_t codegen_get_total_size(void);

// Finalize and get generated code
bool codegen_finalize(uint8_t** code, uint64_t* size);

// Clean up scalable context
void codegen_cleanup(void);

// GGGX function generation
void generate_gggx_function(CodeBuffer* buf, const char* func_name, uint16_t name_len,
                           ASTNode* nodes, uint16_t arg_idx, SymbolTable* symbols, char* string_pool);

#endif // CODEGEN_WRAPPER_H