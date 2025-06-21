// TEMPORAL CODE GENERATION
// Generates machine code that respects time-travel execution order

#include "blaze_internals.h"

// Simple string hash for identifier matching
static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + *str++;
    }
    return hash;
}

// Temporal value storage in x64
typedef struct {
    uint32_t identifier_hash;
    X64Register storage_reg;     // Register holding the value
    int32_t stack_offset;        // Or stack location if spilled
    bool is_computed;
    bool is_future_value;
} TemporalValue;

// Temporal execution context
typedef struct {
    CodeBuffer* code;
    TemporalValue values[32];    // Temporal value storage
    uint8_t value_count;
    
    // Temporal stack frame
    int32_t temporal_stack_size;
    int32_t current_stack_offset;
} TemporalCodeGen;

// Allocate temporal storage
static X64Register allocate_temporal_register(TemporalCodeGen* ctx, uint32_t hash) {
    // Simple allocation: use R12-R15 for temporal values
    static X64Register temporal_regs[] = {R12, R13, R14, R15};
    
    if (ctx->value_count < 4) {
        TemporalValue* val = &ctx->values[ctx->value_count++];
        val->identifier_hash = hash;
        val->storage_reg = temporal_regs[ctx->value_count - 1];
        val->is_computed = false;
        val->is_future_value = true;
        return val->storage_reg;
    }
    
    // Spill to stack if out of registers
    return RAX; // Fallback
}

// Look up temporal value
static TemporalValue* find_temporal_value(TemporalCodeGen* ctx, uint32_t hash) {
    for (uint8_t i = 0; i < ctx->value_count; i++) {
        if (ctx->values[i].identifier_hash == hash) {
            return &ctx->values[i];
        }
    }
    return NULL;
}

// Generate runtime memory operations
static void emit_runtime_store(CodeBuffer* code, const char* var_name, bool to_future) {
    // For time-travel values, allocate in the appropriate temporal zone
    extern void generate_temporal_alloc(CodeBuffer* buf, TimeZone zone, X64Register size_reg, X64Register result_reg);
    
    // Allocate 8 bytes in the future zone for time-travel storage
    if (to_future) {
        emit_mov_reg_imm64(code, RSI, 8); // Size = 8 bytes for a value
        generate_temporal_alloc(code, ZONE_FUTURE, RSI, RAX);
        // RAX now contains pointer to temporal storage
        // Store this pointer for later use
        emit_push_reg(code, RAX);
    }
    
    // Push var name address
    emit_mov_reg_imm64(code, RDI, (uint64_t)var_name);
    
    // Push to_future flag
    emit_mov_reg_imm64(code, RSI, to_future ? 1 : 0);
    
    // Call runtime_store_value
    emit_mov_reg_imm64(code, RAX, (uint64_t)runtime_store_value);
    emit_byte(code, 0xFF);
    emit_byte(code, 0xD0); // CALL RAX
    
    if (to_future) {
        emit_pop_reg(code, RAX); // Restore temporal pointer
    }
}

static void emit_runtime_load(CodeBuffer* code, const char* var_name, bool from_future) {
    // Push var name address
    emit_mov_reg_imm64(code, RDI, (uint64_t)var_name);
    
    // Push from_future flag
    emit_mov_reg_imm64(code, RSI, from_future ? 1 : 0);
    
    // Call runtime_load_value
    emit_mov_reg_imm64(code, RAX, (uint64_t)runtime_load_value);
    emit_byte(code, 0xFF);
    emit_byte(code, 0xD0); // CALL RAX
}

// Generate code for a node in temporal order
static void generate_temporal_node(TemporalCodeGen* ctx, ASTNode* nodes, 
                                 uint16_t node_idx, char* string_pool,
                                 ExecutionStep* step) {
    if (node_idx == 0 || node_idx >= 4096) return;
    
    ASTNode* node = &nodes[node_idx];
    CodeBuffer* code = ctx->code;
    
    // Handle nodes that create future values first
    if (step->creates_past_value) {
        // Mark temporal execution point
        emit_byte(code, 0x90); // NOP as marker
        
        switch (node->type) {
            case NODE_TIMING_OP:
                if (node->data.timing.timing_op == TOK_TIMING_INTO) {
                    // >> operator creates future value
                    uint16_t expr_idx = node->data.timing.expr_idx;
                    if (expr_idx > 0) {
                        // Generate the expression
                        generate_temporal_node(ctx, nodes, expr_idx, string_pool, step);
                        
                        // Store in temporal register
                        ASTNode* expr = &nodes[expr_idx];
                        if (expr->type == NODE_IDENTIFIER) {
                            const char* name = &string_pool[expr->data.ident.name_offset];
                            uint32_t hash = hash_string(name);
                            X64Register temp_reg = allocate_temporal_register(ctx, hash);
                            
                            // Move result to temporal storage
                            emit_mov_reg_reg(code, temp_reg, RAX);
                            
                            // Mark as computed
                            TemporalValue* val = find_temporal_value(ctx, hash);
                            if (val) val->is_computed = true;
                        }
                    }
                }
                break;
                
            case NODE_BINARY_OP:
                if (node->data.binary.op == TOK_GT) {
                    // Assignment that creates future value
                    generate_temporal_node(ctx, nodes, node->data.binary.left_idx, 
                                         string_pool, step);
                    
                    // Store to future identifier
                    uint16_t right_idx = node->data.binary.right_idx;
                    if (right_idx > 0) {
                        ASTNode* right = &nodes[right_idx];
                        if (right->type == NODE_IDENTIFIER) {
                            const char* name = &string_pool[right->data.ident.name_offset];
                            uint32_t hash = hash_string(name);
                            X64Register temp_reg = allocate_temporal_register(ctx, hash);
                            emit_mov_reg_reg(code, temp_reg, RAX);
                            
                            TemporalValue* val = find_temporal_value(ctx, hash);
                            if (val) val->is_computed = true;
                        }
                    }
                }
                break;
        }
    }
    
    // Handle nodes that consume future values
    if (step->requires_future_value) {
        switch (node->type) {
            case NODE_CONDITIONAL:
                // Load future value for condition check
                uint16_t param_idx = node->data.binary.left_idx;
                if (param_idx > 0) {
                    ASTNode* param = &nodes[param_idx];
                    if (param->type == NODE_IDENTIFIER) {
                        const char* name = &string_pool[param->data.ident.name_offset];
                        uint32_t hash = hash_string(name);
                        
                        TemporalValue* val = find_temporal_value(ctx, hash);
                        if (val && val->is_computed) {
                            // Load future value into RAX for comparison
                            emit_mov_reg_reg(code, RAX, val->storage_reg);
                            
                            // Generate condition check
                            emit_cmp_reg_imm32(code, RAX, 30);
                            
                            // Branch based on condition type
                            switch (node->data.binary.op) {
                                case TOK_GREATER_THAN:
                                    emit_jg_rel32(code, 0); // Will be patched
                                    break;
                                case TOK_LESS_EQUAL:
                                    emit_jle_rel32(code, 0); // Will be patched
                                    break;
                            }
                        }
                    }
                }
                break;
                
            case NODE_TIMING_OP:
                if (node->data.timing.timing_op == TOK_LT) {
                    // < operator consumes past/future value
                    uint16_t expr_idx = node->data.timing.expr_idx;
                    if (expr_idx > 0) {
                        ASTNode* expr = &nodes[expr_idx];
                        if (expr->type == NODE_IDENTIFIER) {
                            const char* name = &string_pool[expr->data.ident.name_offset];
                            uint32_t hash = hash_string(name);
                            
                            TemporalValue* val = find_temporal_value(ctx, hash);
                            if (val && val->is_computed) {
                                // Load temporal value
                                emit_mov_reg_reg(code, RAX, val->storage_reg);
                            }
                        }
                    }
                }
                break;
        }
    }
    
    // Generate normal code for non-temporal nodes
    if (!step->creates_past_value && !step->requires_future_value) {
        switch (node->type) {
            case NODE_NUMBER:
                emit_mov_reg_imm64(code, RAX, node->data.number);
                break;
                
            case NODE_IDENTIFIER:
                // For now, just load a test value
                emit_mov_reg_imm64(code, RAX, 42);
                break;
                
            case NODE_ACTION_BLOCK:
                // Generate code for each action
                {
                    uint16_t action = node->data.binary.left_idx;
                    ExecutionStep dummy_step = {0};
                    while (action != 0 && action < 4096) {
                        generate_temporal_node(ctx, nodes, action, string_pool, &dummy_step);
                        action = nodes[action].data.binary.right_idx;
                    }
                }
                break;
        }
    }
}

// Generate function with temporal execution support
void generate_temporal_function(CodeBuffer* code, ASTNode* nodes, uint16_t root_idx,
                              uint16_t node_count, char* string_pool,
                              ExecutionStep* execution_plan, uint32_t plan_size) {
    // Function prologue with extra space for temporal values
    emit_push_reg(code, RBP);
    emit_mov_reg_reg(code, RBP, RSP);
    
    // Reserve stack space (256 bytes for temporal storage)
    emit_rex(code, true, false, false, false);
    emit_byte(code, 0x81);
    emit_byte(code, MODRM(3, 5, RSP));
    emit_dword(code, 256);
    
    // Save callee-saved registers we'll use for temporal values
    emit_push_reg(code, R12);
    emit_push_reg(code, R13);
    emit_push_reg(code, R14);
    emit_push_reg(code, R15);
    
    // Initialize temporal context
    TemporalCodeGen ctx = {
        .code = code,
        .value_count = 0,
        .temporal_stack_size = 256,
        .current_stack_offset = 0
    };
    
    // Execute nodes in temporal order
    for (uint32_t i = 0; i < plan_size; i++) {
        ExecutionStep* step = &execution_plan[i];
        if (step->node_idx > 0) {
            generate_temporal_node(&ctx, nodes, step->node_idx, string_pool, step);
        }
    }
    
    // Example: Print the final result
    emit_mov_reg_imm64(code, RAX, 1);    // sys_write
    emit_mov_reg_imm64(code, RDI, 1);    // stdout
    emit_mov_reg_reg(code, RSI, RSP);    // buffer
    emit_mov_reg_imm64(code, RDX, 16);   // length
    emit_syscall(code);
    
    // Restore registers
    emit_pop_reg(code, R15);
    emit_pop_reg(code, R14);
    emit_pop_reg(code, R13);
    emit_pop_reg(code, R12);
    
    // Function epilogue
    emit_mov_reg_reg(code, RSP, RBP);
    emit_pop_reg(code, RBP);
    emit_byte(code, 0xC3); // RET
}

// Generate code that demonstrates time travel
void generate_time_travel_demo(CodeBuffer* code) {
    // Simple demo that shows future value affecting past execution
    
    // This generates machine code equivalent to:
    // future_value = 42
    // if (future_value > 30) { print("Future says: big!") }
    // else { print("Future says: small!") }
    
    // Set future value in R12
    emit_mov_reg_imm64(code, R12, 42);
    
    // Compare future value
    emit_cmp_reg_imm32(code, R12, 30);
    
    // Jump if greater
    emit_jg_rel32(code, 20); // Skip "small" message
    
    // Print "small" message
    emit_mov_reg_imm64(code, RAX, 1);    // sys_write
    emit_mov_reg_imm64(code, RDI, 1);    // stdout
    emit_mov_reg_imm64(code, RSI, 0);    // Will be patched with string address
    emit_mov_reg_imm64(code, RDX, 18);   // "Future says: small!"
    emit_syscall(code);
    emit_jmp_rel32(code, 15); // Skip "big" message
    
    // Print "big" message
    emit_mov_reg_imm64(code, RAX, 1);    // sys_write
    emit_mov_reg_imm64(code, RDI, 1);    // stdout
    emit_mov_reg_imm64(code, RSI, 0);    // Will be patched with string address
    emit_mov_reg_imm64(code, RDX, 16);   // "Future says: big!"
    emit_syscall(code);
    
    // Return
    emit_byte(code, 0xC3);
}