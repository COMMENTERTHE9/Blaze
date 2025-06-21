// Simple float print for debugging
#include "blaze_internals.h"

// Generate code to print a float from XMM0
void generate_print_float(CodeBuffer* buf) {
    // For now, just print "3.14\n" as a test
    
    // Store the string inline
    emit_byte(buf, 0xEB); // JMP over data
    emit_byte(buf, 0x05); // Skip 5 bytes
    
    uint32_t string_pos = buf->position;
    emit_byte(buf, '3');
    emit_byte(buf, '.');
    emit_byte(buf, '1');
    emit_byte(buf, '4');
    emit_byte(buf, '\n');
    
    // MOV RAX, 1 (sys_write)
    emit_byte(buf, 0x48); // REX.W
    emit_byte(buf, 0xB8); // MOV RAX, imm64
    emit_qword(buf, 1);
    
    // MOV RDI, 1 (stdout)
    emit_byte(buf, 0x48); // REX.W
    emit_byte(buf, 0xBF); // MOV RDI, imm64
    emit_qword(buf, 1);
    
    // LEA RSI, [RIP + offset]
    emit_byte(buf, 0x48); // REX.W
    emit_byte(buf, 0x8D); // LEA
    emit_byte(buf, 0x35); // ModRM for [RIP+disp32]
    int32_t offset = string_pos - (buf->position + 4);
    emit_dword(buf, offset);
    
    // MOV RDX, 5 (length)
    emit_byte(buf, 0x48); // REX.W
    emit_byte(buf, 0xBA); // MOV RDX, imm64
    emit_qword(buf, 5);
    
    // SYSCALL
    emit_byte(buf, 0x0F);
    emit_byte(buf, 0x05);
}