// Simplest possible hello world for testing

#include "blaze_internals.h"

extern void emit_byte(CodeBuffer* buf, uint8_t byte);

void generate_hello_simple(CodeBuffer* buf) {
    // Jump over the data
    emit_byte(buf, 0xeb); // jmp short
    emit_byte(buf, 0x0d); // skip 13 bytes
    
    // Data: "Hello World!\n"
    emit_byte(buf, 'H'); emit_byte(buf, 'e'); emit_byte(buf, 'l'); emit_byte(buf, 'l');
    emit_byte(buf, 'o'); emit_byte(buf, ' '); emit_byte(buf, 'W'); emit_byte(buf, 'o');
    emit_byte(buf, 'r'); emit_byte(buf, 'l'); emit_byte(buf, 'd'); emit_byte(buf, '!');
    emit_byte(buf, '\n');
    
    // Code starts here
    // mov rax, 1 (sys_write)
    emit_byte(buf, 0x48); emit_byte(buf, 0xc7); emit_byte(buf, 0xc0);
    emit_byte(buf, 0x01); emit_byte(buf, 0x00); emit_byte(buf, 0x00); emit_byte(buf, 0x00);
    
    // mov rdi, 1 (stdout) 
    emit_byte(buf, 0x48); emit_byte(buf, 0xc7); emit_byte(buf, 0xc7);
    emit_byte(buf, 0x01); emit_byte(buf, 0x00); emit_byte(buf, 0x00); emit_byte(buf, 0x00);
    
    // lea rsi, [rip-33] (point back to "Hello World!\n")
    emit_byte(buf, 0x48); emit_byte(buf, 0x8d); emit_byte(buf, 0x35);
    emit_byte(buf, 0xdf); emit_byte(buf, 0xff); emit_byte(buf, 0xff); emit_byte(buf, 0xff);
    
    // mov rdx, 13 (length)
    emit_byte(buf, 0x48); emit_byte(buf, 0xc7); emit_byte(buf, 0xc2);
    emit_byte(buf, 0x0d); emit_byte(buf, 0x00); emit_byte(buf, 0x00); emit_byte(buf, 0x00);
    
    // syscall
    emit_byte(buf, 0x0f); emit_byte(buf, 0x05);
    
    // mov rax, 60 (sys_exit)
    emit_byte(buf, 0x48); emit_byte(buf, 0xc7); emit_byte(buf, 0xc0);
    emit_byte(buf, 0x3c); emit_byte(buf, 0x00); emit_byte(buf, 0x00); emit_byte(buf, 0x00);
    
    // xor rdi, rdi
    emit_byte(buf, 0x48); emit_byte(buf, 0x31); emit_byte(buf, 0xff);
    
    // syscall
    emit_byte(buf, 0x0f); emit_byte(buf, 0x05);
}