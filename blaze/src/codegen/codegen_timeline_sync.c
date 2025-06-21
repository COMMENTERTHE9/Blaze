// Code generation for timeline synchronization (fixed points and permanent flow)
// Generates x64 assembly for Blaze timeline sync features

#include "blaze_internals.h"

// External functions from runtime
extern uint64_t register_fixedpoint(const char* name);
extern void timeline_arrive_fixedpoint(uint64_t fixpoint_id, uint64_t timeline_id, void* timeline_data);
extern uint64_t register_permanent_timeline(uint64_t timeline_id, uint64_t rate_hz);
extern bool should_execute_flow(uint64_t flow_id);

// Generate code for fixed point definition
void gen_fixedpoint_def(uint8_t* output, uint32_t* offset, ASTNode* node, 
                       char* string_pool, SymbolTable* symbols) {
    // Get fixed point name
    uint16_t name_idx = node->data.fixed_point.name_idx;
    char* fp_name = NULL;
    
    if (name_idx != 0xFFFF && name_idx < 4096) {
        // name_idx is actually an index into the nodes array, not relative to current node
        // So we need access to the base nodes array, which should be passed as parameter
        // For now, just use a placeholder
        fp_name = "fixedpoint_name"; // TODO: resolve from symbol table
    }
    
    // Generate call to register_fixedpoint
    // MOV RDI, name_string_addr
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0xBF;
    *(uint64_t*)(output + *offset) = (uint64_t)fp_name;
    *offset += 8;
    
    // CALL register_fixedpoint
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0xB8;
    *(uint64_t*)(output + *offset) = (uint64_t)register_fixedpoint;
    *offset += 8;
    output[(*offset)++] = 0xFF;
    output[(*offset)++] = 0xD0;
    
    // Store result (fixpoint ID) in symbol table
    // For now, store in RAX
}

// Generate code for arriving at fixed point
void gen_fixedpoint_arrive(uint8_t* output, uint32_t* offset, uint64_t fp_id,
                          uint64_t timeline_id) {
    // MOV RDI, fixpoint_id
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0xBF;
    *(uint64_t*)(output + *offset) = fp_id;
    *offset += 8;
    
    // MOV RSI, timeline_id
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0xBE;
    *(uint64_t*)(output + *offset) = timeline_id;
    *offset += 8;
    
    // MOV RDX, timeline_data (use current stack pointer as data)
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0x89;
    output[(*offset)++] = 0xE2;  // MOV RDX, RSP
    
    // CALL timeline_arrive_fixedpoint
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0xB8;
    *(uint64_t*)(output + *offset) = (uint64_t)timeline_arrive_fixedpoint;
    *offset += 8;
    output[(*offset)++] = 0xFF;
    output[(*offset)++] = 0xD0;
}

// Generate code for permanent timeline definition
void gen_permanent_timeline(uint8_t* output, uint32_t* offset, ASTNode* node,
                          char* string_pool, SymbolTable* symbols) {
    uint64_t timeline_id = 0; // Would be assigned by runtime
    uint64_t rate_hz = 0;
    
    // Check if this has a flow spec (rate control)
    if (node->type == NODE_FLOW_SPEC) {
        rate_hz = node->data.flow_spec.rate;
        timeline_id = node->data.flow_spec.timeline_idx;
    }
    
    // MOV RDI, timeline_id
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0xBF;
    *(uint64_t*)(output + *offset) = timeline_id;
    *offset += 8;
    
    // MOV RSI, rate_hz
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0xBE;
    *(uint64_t*)(output + *offset) = rate_hz;
    *offset += 8;
    
    // CALL register_permanent_timeline
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0xB8;
    *(uint64_t*)(output + *offset) = (uint64_t)register_permanent_timeline;
    *offset += 8;
    output[(*offset)++] = 0xFF;
    output[(*offset)++] = 0xD0;
    
    // Store flow ID in RAX
}

// Generate permanent timeline execution loop
void gen_permanent_loop(uint8_t* output, uint32_t* offset, uint64_t flow_id,
                       uint64_t target_function) {
    // Loop start
    uint32_t loop_start = *offset;
    
    // Check if should execute
    // MOV RDI, flow_id
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0xBF;
    *(uint64_t*)(output + *offset) = flow_id;
    *offset += 8;
    
    // CALL should_execute_flow
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0xB8;
    *(uint64_t*)(output + *offset) = (uint64_t)should_execute_flow;
    *offset += 8;
    output[(*offset)++] = 0xFF;
    output[(*offset)++] = 0xD0;
    
    // TEST AL, AL
    output[(*offset)++] = 0x84;
    output[(*offset)++] = 0xC0;
    
    // JZ skip_execution
    output[(*offset)++] = 0x74;
    uint8_t skip_offset = *offset;
    (*offset)++;
    
    // Call target function
    output[(*offset)++] = 0x48;
    output[(*offset)++] = 0xB8;
    *(uint64_t*)(output + *offset) = target_function;
    *offset += 8;
    output[(*offset)++] = 0xFF;
    output[(*offset)++] = 0xD0;
    
    // skip_execution:
    output[skip_offset] = *offset - (skip_offset + 1);
    
    // Small delay to prevent CPU spinning (optional)
    // PAUSE instruction
    output[(*offset)++] = 0xF3;
    output[(*offset)++] = 0x90;
    
    // JMP loop_start
    output[(*offset)++] = 0xE9;
    *(int32_t*)(output + *offset) = loop_start - (*offset + 4);
    *offset += 4;
}

// Main code generation entry for sync operations
void gen_timeline_sync_operation(uint8_t* output, uint32_t* offset, ASTNode* node,
                               char* string_pool, SymbolTable* symbols) {
    switch (node->type) {
        case NODE_FIXED_POINT:
            gen_fixedpoint_def(output, offset, node, string_pool, symbols);
            break;
            
        case NODE_PERMANENT_TIMELINE:
            gen_permanent_timeline(output, offset, node, string_pool, symbols);
            break;
            
        case NODE_FLOW_SPEC:
            gen_permanent_timeline(output, offset, node, string_pool, symbols);
            break;
            
        default:
            // Handle timeline jumps with fixed points
            if (node->type == NODE_JUMP && node->data.timing.expr_idx != 0xFFFF) {
                // Check if target is a fixed point
                // This would involve checking the expression tree
            }
            break;
    }
}

// Generate synchronization chain (e.g., state >> f.p >> next_state)
void gen_sync_chain(uint8_t* output, uint32_t* offset, uint16_t* chain_nodes,
                   uint16_t chain_length, ASTNode* nodes, char* string_pool) {
    for (int i = 0; i < chain_length; i++) {
        ASTNode* node = &nodes[chain_nodes[i]];
        
        if (node->type == NODE_FIXED_POINT) {
            // Arrive at fixed point
            uint64_t fp_id = 0; // Would be resolved from symbol table
            uint64_t timeline_id = 0; // Current timeline ID
            gen_fixedpoint_arrive(output, offset, fp_id, timeline_id);
        }
        else {
            // Execute normal node
            // This would call the appropriate code generation
        }
    }
}