// Simple hello world code generation for testing

#include "blaze_internals.h"

// Forward declarations
extern void emit_byte(CodeBuffer* buf, uint8_t byte);

// Generate a simple hello world program
void generate_hello_world(CodeBuffer* buf) {
    // The complete hello world program in machine code
    // This writes "Hello World!\n" and exits
    
    // mov rax, 1 (sys_write)
    emit_byte(buf, 0x48); emit_byte(buf, 0xc7); emit_byte(buf, 0xc0);
    emit_byte(buf, 0x01); emit_byte(buf, 0x00); emit_byte(buf, 0x00); emit_byte(buf, 0x00);
    
    // mov rdi, 1 (stdout)
    emit_byte(buf, 0x48); emit_byte(buf, 0xc7); emit_byte(buf, 0xc7);
    emit_byte(buf, 0x01); emit_byte(buf, 0x00); emit_byte(buf, 0x00); emit_byte(buf, 0x00);
    
    // lea rsi, [rip+0x13] (message address - adjust for actual distance)
    emit_byte(buf, 0x48); emit_byte(buf, 0x8d); emit_byte(buf, 0x35);
    emit_byte(buf, 0x13); emit_byte(buf, 0x00); emit_byte(buf, 0x00); emit_byte(buf, 0x00);
    
    // mov rdx, 13 (length)
    emit_byte(buf, 0x48); emit_byte(buf, 0xc7); emit_byte(buf, 0xc2);
    emit_byte(buf, 0x0d); emit_byte(buf, 0x00); emit_byte(buf, 0x00); emit_byte(buf, 0x00);
    
    // syscall
    emit_byte(buf, 0x0f); emit_byte(buf, 0x05);
    
    // mov rax, 60 (sys_exit)
    emit_byte(buf, 0x48); emit_byte(buf, 0xc7); emit_byte(buf, 0xc0);
    emit_byte(buf, 0x3c); emit_byte(buf, 0x00); emit_byte(buf, 0x00); emit_byte(buf, 0x00);
    
    // xor rdi, rdi (exit code 0)
    emit_byte(buf, 0x48); emit_byte(buf, 0x31); emit_byte(buf, 0xff);
    
    // syscall
    emit_byte(buf, 0x0f); emit_byte(buf, 0x05);
    
    // The message data
    emit_byte(buf, 'H'); emit_byte(buf, 'e'); emit_byte(buf, 'l'); emit_byte(buf, 'l');
    emit_byte(buf, 'o'); emit_byte(buf, ' '); emit_byte(buf, 'W'); emit_byte(buf, 'o');
    emit_byte(buf, 'r'); emit_byte(buf, 'l'); emit_byte(buf, 'd'); emit_byte(buf, '!');
    emit_byte(buf, '\n');
}