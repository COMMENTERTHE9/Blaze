// Timeline bounce code generation for Blaze
// Generates x64 assembly for timeline collision handling

#include "blaze_internals.h"

// Timeline state structure in memory
// [8 bytes] timeline_id
// [8 bytes] target_state_address
// [8 bytes] collision_strategy (0=bounce, 1=merge, 2=queue)
// [8 bytes] next_timeline_ptr
// [256 bytes] timeline_data

#define TIMELINE_STRUCT_SIZE 280
#define TIMELINE_ID_OFFSET 0
#define TIMELINE_TARGET_OFFSET 8
#define TIMELINE_STRATEGY_OFFSET 16
#define TIMELINE_NEXT_OFFSET 24
#define TIMELINE_DATA_OFFSET 32

// Global timeline collision table
// Located at fixed memory address for runtime access
#define COLLISION_TABLE_ADDR 0x500000
#define MAX_TIMELINES 1024

// Generate collision detection code
void gen_collision_detect(uint8_t* output, uint32_t* offset, uint64_t target_addr) {
    // Load collision table base
    output[(*offset)++] = 0x48; // MOV RBX, imm64
    output[(*offset)++] = 0xBB;
    *(uint64_t*)(output + *offset) = COLLISION_TABLE_ADDR;
    *offset += 8;
    
    // Load target address into RCX
    output[(*offset)++] = 0x48; // MOV RCX, imm64
    output[(*offset)++] = 0xB9;
    *(uint64_t*)(output + *offset) = target_addr;
    *offset += 8;
    
    // Scan collision table for matching target
    // XOR RSI, RSI (counter)
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x31;
    output[(*offset)++] = 0xF6;
    
    // Loop start
    uint32_t loop_start = *offset;
    
    // CMP RSI, MAX_TIMELINES
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x81;
    output[(*offset)++] = 0xFE;
    *(uint32_t*)(output + *offset) = MAX_TIMELINES;
    *offset += 4;
    
    // JGE done (no collision)
    output[(*offset)++] = 0x0F;
    output[(*offset)++] = 0x8D;
    uint32_t jmp_done = *offset;
    *offset += 4;
    
    // Calculate entry address: RBX + RSI * TIMELINE_STRUCT_SIZE
    // MOV RAX, RSI
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x89;
    output[(*offset)++] = 0xF0;
    
    // IMUL RAX, TIMELINE_STRUCT_SIZE
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x69;
    output[(*offset)++] = 0xC0;
    *(uint32_t*)(output + *offset) = TIMELINE_STRUCT_SIZE;
    *offset += 4;
    
    // ADD RAX, RBX
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x01;
    output[(*offset)++] = 0xD8;
    
    // Load target from entry
    // MOV RDX, [RAX + TIMELINE_TARGET_OFFSET]
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x8B;
    output[(*offset)++] = 0x50;
    output[(*offset)++] = TIMELINE_TARGET_OFFSET;
    
    // CMP RDX, RCX (compare targets)
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x39;
    output[(*offset)++] = 0xCA;
    
    // JE collision_found
    output[(*offset)++] = 0x74;
    uint8_t jmp_collision = *offset;
    *offset += 1;
    
    // INC RSI
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0xFF;
    output[(*offset)++] = 0xC6;
    
    // JMP loop_start
    output[(*offset)++] = 0xE9;
    *(int32_t*)(output + *offset) = loop_start - (*offset + 4);
    *offset += 4;
    
    // Collision found - RAX points to existing timeline
    output[jmp_collision] = *offset - (jmp_collision + 1);
    
    // Load collision strategy
    // MOV RDI, [RAX + TIMELINE_STRATEGY_OFFSET]
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x8B;
    output[(*offset)++] = 0x78;
    output[(*offset)++] = TIMELINE_STRATEGY_OFFSET;
    
    // No collision path
    uint32_t done_offset = *offset;
    *(int32_t*)(output + jmp_done) = done_offset - (jmp_done + 4);
}

// Generate bounce operation
void gen_timeline_bounce(uint8_t* output, uint32_t* offset, uint64_t bounce_target) {
    // Assumes RAX contains timeline to bounce
    // RDI contains strategy (should be 0 for bounce)
    
    // Find alternate state for bounced timeline
    // This is a simplified version - real implementation would be more complex
    
    // MOV RCX, bounce_target
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0xB9;
    *(uint64_t*)(output + *offset) = bounce_target;
    *offset += 8;
    
    // Update timeline's target
    // MOV [RAX + TIMELINE_TARGET_OFFSET], RCX
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x89;
    output[(*offset)++] = 0x48;
    output[(*offset)++] = TIMELINE_TARGET_OFFSET;
    
    // Signal bounce to runtime
    // MOV RAX, 1 (bounce occurred)
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0xC7;
    output[(*offset)++] = 0xC0;
    *(uint32_t*)(output + *offset) = 1;
    *offset += 4;
}

// Generate merge operation
void gen_timeline_merge(uint8_t* output, uint32_t* offset) {
    // Assumes RAX contains existing timeline
    // RBX contains new timeline data
    
    // Merge algorithm:
    // 1. Average numeric values
    // 2. Concatenate strings
    // 3. Combine flags with OR
    
    // For now, simple implementation that copies new data
    // MOV RDI, RAX (destination)
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x89;
    output[(*offset)++] = 0xC7;
    
    // ADD RDI, TIMELINE_DATA_OFFSET
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x83;
    output[(*offset)++] = 0xC7;
    output[(*offset)++] = TIMELINE_DATA_OFFSET;
    
    // MOV RSI, RBX (source)
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x89;
    output[(*offset)++] = 0xDE;
    
    // MOV RCX, 256 (data size)
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0xC7;
    output[(*offset)++] = 0xC1;
    *(uint32_t*)(output + *offset) = 256;
    *offset += 4;
    
    // REP MOVSB (copy data)
    output[(*offset)++] = 0xF3;
    output[(*offset)++] = 0xA4;
}

// Generate queue operation
void gen_timeline_queue(uint8_t* output, uint32_t* offset) {
    // Assumes RAX contains existing timeline
    // RBX contains new timeline to queue
    
    // Find end of queue
    // MOV RDI, RAX
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x89;
    output[(*offset)++] = 0xC7;
    
    // Loop to find last timeline in queue
    uint32_t queue_loop = *offset;
    
    // MOV RSI, [RDI + TIMELINE_NEXT_OFFSET]
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x8B;
    output[(*offset)++] = 0x77;
    output[(*offset)++] = TIMELINE_NEXT_OFFSET;
    
    // TEST RSI, RSI
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x85;
    output[(*offset)++] = 0xF6;
    
    // JZ found_end
    output[(*offset)++] = 0x74;
    uint8_t jmp_found = *offset;
    *offset += 1;
    
    // MOV RDI, RSI (advance to next)
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x89;
    output[(*offset)++] = 0xF7;
    
    // JMP queue_loop
    output[(*offset)++] = 0xEB;
    output[(*offset)++] = queue_loop - (*offset + 1);
    
    // Found end - link new timeline
    output[jmp_found] = *offset - (jmp_found + 1);
    
    // MOV [RDI + TIMELINE_NEXT_OFFSET], RBX
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x89;
    output[(*offset)++] = 0x5F;
    output[(*offset)++] = TIMELINE_NEXT_OFFSET;
}

// Main code generation for timeline operations
void gen_timeline_operation(uint8_t* output, uint32_t* offset, ASTNode* node, 
                           char* string_pool, SymbolTable* symbols) {
    // Generate collision detection
    uint64_t target_addr = 0x400000; // Default target
    
    if (node->data.timing.expr_idx != 0xFFFF) {
        // Calculate actual target address from expression
        // This would involve resolving the identifier or expression
    }
    
    gen_collision_detect(output, offset, target_addr);
    
    // Check operation type
    if (node->data.timing.timing_op == TOK_BNC) {
        // Bounce operation
        uint64_t bounce_target = 0x401000; // Default bounce location
        gen_timeline_bounce(output, offset, bounce_target);
    }
    else if (node->data.timing.timing_op == TOK_RECV) {
        // Merge operation
        gen_timeline_merge(output, offset);
    }
    else if (node->data.timing.timing_op == TOK_RECV + 1) {
        // Queue operation
        gen_timeline_queue(output, offset);
    }
}