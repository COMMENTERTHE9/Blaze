// Inline assembly code generation for Blaze
// Handles asm/ blocks in Blaze programs

#include "blaze_internals.h"

// Helper to parse hex byte from string (e.g., "48" -> 0x48)
static uint8_t parse_hex_byte(const char* str) {
    uint8_t result = 0;
    for (int i = 0; i < 2 && str[i]; i++) {
        result *= 16;
        if (str[i] >= '0' && str[i] <= '9') {
            result += str[i] - '0';
        } else if (str[i] >= 'a' && str[i] <= 'f') {
            result += str[i] - 'a' + 10;
        } else if (str[i] >= 'A' && str[i] <= 'F') {
            result += str[i] - 'A' + 10;
        }
    }
    return result;
}

// Process assembly mnemonics to machine code
static void assemble_instruction(const char* mnemonic, uint8_t* output, uint32_t* offset) {
    // Simple assembler for common x64 instructions
    // This is a minimal implementation - real assembler would be more complex
    
    // Skip whitespace
    while (*mnemonic == ' ' || *mnemonic == '\t') mnemonic++;
    
    // NOP
    if (mnemonic[0] == 'n' && mnemonic[1] == 'o' && mnemonic[2] == 'p') {
        output[(*offset)++] = 0x90;
        return;
    }
    
    // RET
    if (mnemonic[0] == 'r' && mnemonic[1] == 'e' && mnemonic[2] == 't') {
        output[(*offset)++] = 0xC3;
        return;
    }
    
    // SYSCALL
    if (mnemonic[0] == 's' && mnemonic[1] == 'y' && mnemonic[2] == 's' &&
        mnemonic[3] == 'c' && mnemonic[4] == 'a' && mnemonic[5] == 'l' && mnemonic[6] == 'l') {
        output[(*offset)++] = 0x0F;
        output[(*offset)++] = 0x05;
        return;
    }
    
    // MOV RAX, <imm64>
    if (mnemonic[0] == 'm' && mnemonic[1] == 'o' && mnemonic[2] == 'v' && 
        mnemonic[3] == ' ' && mnemonic[4] == 'r' && mnemonic[5] == 'a' && mnemonic[6] == 'x') {
        const char* ptr = mnemonic + 7;
        while (*ptr == ' ' || *ptr == ',') ptr++;
        
        // Parse immediate value
        uint64_t value = 0;
        bool is_hex = false;
        if (ptr[0] == '0' && ptr[1] == 'x') {
            is_hex = true;
            ptr += 2;
        }
        
        while (*ptr && *ptr != ' ' && *ptr != '\n') {
            if (is_hex) {
                value *= 16;
                if (*ptr >= '0' && *ptr <= '9') value += *ptr - '0';
                else if (*ptr >= 'a' && *ptr <= 'f') value += *ptr - 'a' + 10;
                else if (*ptr >= 'A' && *ptr <= 'F') value += *ptr - 'A' + 10;
            } else {
                value = value * 10 + (*ptr - '0');
            }
            ptr++;
        }
        
        // Emit MOV RAX, imm64
        output[(*offset)++] = 0x48;  // REX.W
        output[(*offset)++] = 0xB8;  // MOV RAX, imm64
        *(uint64_t*)(output + *offset) = value;
        *offset += 8;
        return;
    }
    
    // INT3 (breakpoint)
    if (mnemonic[0] == 'i' && mnemonic[1] == 'n' && mnemonic[2] == 't' && mnemonic[3] == '3') {
        output[(*offset)++] = 0xCC;
        return;
    }
}

// Generate inline assembly
void gen_inline_asm(uint8_t* output, uint32_t* offset, ASTNode* node, 
                   char* string_pool, SymbolTable* symbols) {
    // Get assembly code from string pool
    char* asm_code = &string_pool[node->data.inline_asm.code_offset];
    uint16_t asm_len = node->data.inline_asm.code_len;
    
    // Process the assembly code
    // It can be in different formats:
    // 1. Raw hex bytes: "48 89 E5" or "4889E5"
    // 2. Assembly mnemonics: "mov rax, 1\nsyscall"
    // 3. Mixed format
    
    uint32_t i = 0;
    while (i < asm_len) {
        // Skip whitespace
        while (i < asm_len && (asm_code[i] == ' ' || asm_code[i] == '\t' || 
                               asm_code[i] == '\n' || asm_code[i] == '\r')) {
            i++;
        }
        
        if (i >= asm_len) break;
        
        // Check if this looks like hex (starts with digit or A-F)
        if ((asm_code[i] >= '0' && asm_code[i] <= '9') ||
            (asm_code[i] >= 'a' && asm_code[i] <= 'f') ||
            (asm_code[i] >= 'A' && asm_code[i] <= 'F')) {
            
            // Try to parse as hex byte
            if (i + 1 < asm_len) {
                uint8_t byte = parse_hex_byte(&asm_code[i]);
                output[(*offset)++] = byte;
                i += 2;
                
                // Skip optional space after hex byte
                if (i < asm_len && asm_code[i] == ' ') i++;
            } else {
                i++; // Skip invalid single hex digit
            }
        } else {
            // Parse as assembly mnemonic
            // Find end of line
            uint32_t line_start = i;
            while (i < asm_len && asm_code[i] != '\n' && asm_code[i] != ';') {
                i++;
            }
            
            // Create temporary null-terminated string for the line
            char line_buffer[256];
            uint32_t line_len = i - line_start;
            if (line_len > 255) line_len = 255;
            
            for (uint32_t j = 0; j < line_len; j++) {
                line_buffer[j] = asm_code[line_start + j];
            }
            line_buffer[line_len] = '\0';
            
            // Assemble the instruction
            assemble_instruction(line_buffer, output, offset);
            
            // Skip to next line
            if (i < asm_len && asm_code[i] == ';') {
                // Skip comment
                while (i < asm_len && asm_code[i] != '\n') i++;
            }
            if (i < asm_len && asm_code[i] == '\n') i++;
        }
    }
}