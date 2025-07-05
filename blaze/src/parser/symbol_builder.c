// SYMBOL BUILDER - Populates symbol table from AST
// Handles temporal scoping and forward references

#include "blaze_internals.h"

// Forward declarations from symbol_table.c
void symbol_table_init(SymbolTable* table, char* string_pool);
void symbol_push_scope(SymbolTable* table, bool is_temporal, int32_t temporal_shift);
void symbol_pop_scope(SymbolTable* table);
Symbol* symbol_add_variable(SymbolTable* table, const char* name, uint16_t name_len,
                           bool is_temporal, bool is_mutable);
Symbol* symbol_add_function(SymbolTable* table, const char* name, uint16_t name_len,
                           uint16_t ast_node, uint8_t param_count);
// symbol_add_array_4d is declared in blaze_internals.h
Symbol* symbol_lookup(SymbolTable* table, const char* name, uint16_t name_len,
                     bool from_future);
void symbol_mark_temporal(Symbol* sym);

// Builder state
typedef struct {
    SymbolTable* table;
    ASTNode* nodes;
    char* string_pool;
    uint16_t node_count;
    
    // Error tracking
    bool has_error;
    uint16_t error_node;
} SymbolBuilder;

// Build symbols from AST node
static void build_symbols_from_node(SymbolBuilder* builder, uint16_t node_idx);

// Process 4D array definition
static void process_array_4d_def(SymbolBuilder* builder, uint16_t node_idx) {
    ASTNode* node = &builder->nodes[node_idx];
    
    // Get array name
    uint16_t name_idx = node->data.array_4d.name_idx;
    if (name_idx == 0 || name_idx >= builder->node_count) return;
    
    ASTNode* name_node = &builder->nodes[name_idx];
    const char* name = &builder->string_pool[name_node->data.ident.name_offset];
    
    // Evaluate dimensions (for now, assume constants)
    uint32_t dimensions[4];
    for (int i = 0; i < 4; i++) {
        uint16_t dim_idx = node->data.array_4d.dim_indices[i];
        if (dim_idx > 0 && builder->nodes[dim_idx].type == NODE_NUMBER) {
            dimensions[i] = (uint32_t)builder->nodes[dim_idx].data.number;
        } else {
            dimensions[i] = 10; // Default size
        }
    }
    
    // Add to symbol table
    symbol_add_array_4d(builder->table, name, 
                       dimensions[0], dimensions[1], dimensions[2], dimensions[3]);
}

// Process variable definition
static void process_var_def(SymbolBuilder* builder, uint16_t node_idx) {
    print_str("[SYMBOL] process_var_def called with node_idx=");
    print_num(node_idx);
    print_str("\n");
    
    // Bounds check
    if (node_idx >= builder->node_count) {
        print_str("[SYMBOL] ERROR: node_idx=");
        print_num(node_idx);
        print_str(" exceeds node_count=");
        print_num(builder->node_count);
        print_str("\n");
        builder->has_error = true;
        return;
    }
    
    ASTNode* node = &builder->nodes[node_idx];
    
    print_str("[SYMBOL] name_offset=");
    print_num(node->data.ident.name_offset);
    print_str(" name_len field=");
    print_num(node->data.ident.name_len);
    print_str("\n");
    
    // Get variable name
    const char* name = &builder->string_pool[node->data.ident.name_offset];
    uint16_t name_len = node->data.ident.name_len & 0xFFFF; // Lower 16 bits
    
    // Sanity check name_len
    if (name_len > 256) {
        print_str("[SYMBOL] ERROR: Invalid name_len=");
        print_num(name_len);
        print_str("\n");
        builder->has_error = true;
        return;
    }
    
    print_str("[SYMBOL] Variable name: ");
    for (uint16_t i = 0; i < name_len && i < 32; i++) {  // Limit output
        char ch[2] = {name[i], '\0'};
        print_str(ch);
    }
    if (name_len > 32) print_str("...");
    print_str(" (len=");
    print_num(name_len);
    print_str(")\n");
    
    // Check for redefinition in current scope
    Symbol* existing = symbol_lookup(builder->table, name, name_len, false);
    if (existing && existing->scope_level == builder->table->current_scope) {
        builder->has_error = true;
        builder->error_node = node_idx;
        return;
    }
    
    // Determine if temporal (has time-travel operators)
    bool is_temporal = false;
    uint16_t init_idx = node->data.ident.name_len >> 16; // Upper 16 bits
    
    if (init_idx > 0 && init_idx < builder->node_count) {
        // Check initializer for temporal operations
        ASTNode* init = &builder->nodes[init_idx];
        if (init->type == NODE_TIMING_OP) {
            is_temporal = true;
        }
    }
    
    // Add to symbol table
    Symbol* sym = symbol_add_variable(builder->table, name, name_len, 
                                     is_temporal, true); // All vars mutable for now
    
    if (!sym) {
        builder->has_error = true;
        builder->error_node = node_idx;
        return;
    }
    
    // Process initializer
    if (init_idx > 0) {
        build_symbols_from_node(builder, init_idx);
    }
}

// Process function definition
static void process_func_def(SymbolBuilder* builder, uint16_t node_idx) {
    ASTNode* node = &builder->nodes[node_idx];
    
    // Get function name from expr_idx
    uint16_t name_idx = node->data.timing.expr_idx;
    if (name_idx == 0 || name_idx >= builder->node_count) {
        builder->has_error = true;
        builder->error_node = node_idx;
        return;
    }
    
    ASTNode* name_node = &builder->nodes[name_idx];
    if (name_node->type != NODE_IDENTIFIER) {
        builder->has_error = true;
        builder->error_node = node_idx;
        return;
    }
    
    const char* name = &builder->string_pool[name_node->data.ident.name_offset];
    uint16_t name_len = name_node->data.ident.name_len;
    
    // Count parameters
    uint16_t param_count = node->data.binary.op;
    uint16_t current_param = 0;
    uint16_t param_idx = node->data.binary.left_idx; // First parameter
    
    // Count actual parameters
    while (param_idx != 0 && current_param < 6 && param_idx < builder->node_count) {
        current_param++;
        ASTNode* param_node = &builder->nodes[param_idx];
        param_idx = param_node->data.binary.right_idx; // Next parameter
    }
    
    // Add function to symbol table
    Symbol* sym = symbol_add_function(builder->table, name, name_len, 
                                     node_idx, current_param);
    
    if (!sym) {
        builder->has_error = true;
        builder->error_node = node_idx;
        return;
    }
    
    // Check for temporal markers
    if (node->data.timing.timing_op != 0) {
        symbol_mark_temporal(sym);
    }
    
    // Create new scope for function body
    symbol_push_scope(builder->table, false, 0);
    
    // Process function parameters
    param_idx = node->data.binary.left_idx; // Reset to first parameter
    current_param = 0;
    
    while (param_idx != 0 && current_param < 6 && param_idx < builder->node_count) {
        ASTNode* param_node = &builder->nodes[param_idx];
        if (param_node->type == NODE_IDENTIFIER) {
            // Parameter has name stored in binary structure
            uint32_t name_offset = param_node->data.binary.left_idx;
            const char* param_name = &builder->string_pool[name_offset];
            uint16_t param_name_len = param_node->data.ident.name_len;
            
            // Add parameter to symbol table
            Symbol* param_sym = symbol_add_variable(builder->table, param_name, param_name_len, 
                                                   false, true); // Not temporal, mutable
            if (param_sym) {
                // Mark as function parameter
                param_sym->type = SYMBOL_VARIABLE;
                param_sym->storage = STORAGE_REGISTER;
                param_sym->data.var.is_mutable = true;
                
                print_str("  Added function parameter: ");
                print_str(param_name);
                print_str("\n");
            }
        }
        
        // Move to next parameter
        param_idx = param_node->data.binary.right_idx;
        current_param++;
    }
    
    // Process function body
    uint16_t body_idx = node->data.timing.temporal_offset;
    if (body_idx != 0 && body_idx < builder->node_count) {
        build_symbols_from_node(builder, body_idx);
    }
    
    // Pop function scope
    symbol_pop_scope(builder->table);
}

// Process array definition
// Currently unused - kept for future implementation
#if 0
static void process_array_4d(SymbolBuilder* builder, uint16_t node_idx) {
    ASTNode* node = &builder->nodes[node_idx];
    
    // Get array name
    uint16_t name_idx = node->data.array_4d.name_idx;
    if (name_idx > 0 && name_idx < builder->node_count) {
        ASTNode* name_node = &builder->nodes[name_idx];
        if (name_node->type == NODE_IDENTIFIER) {
            const char* name = &builder->string_pool[name_node->data.ident.name_offset];
            
            // Get dimensions (would need to evaluate expressions)
            uint32_t dims[4] = {10, 10, 10, 10}; // Default for now
            
            Symbol* sym = symbol_add_array_4d(builder->table, name, 
                                             dims[0], dims[1], dims[2], dims[3]);
            if (!sym) {
                builder->has_error = true;
                builder->error_node = node_idx;
            }
        }
    }
}
#endif

// Process identifier reference
static void process_identifier(SymbolBuilder* builder, uint16_t node_idx) {
    ASTNode* node = &builder->nodes[node_idx];
    
    const char* name = &builder->string_pool[node->data.ident.name_offset];
    uint16_t name_len = node->data.ident.name_len;
    
    // Look up symbol
    Symbol* sym = symbol_lookup(builder->table, name, name_len, false);
    
    if (!sym) {
        // Check if it's a forward reference (temporal)
        sym = symbol_lookup(builder->table, name, name_len, true);
        
        if (!sym) {
            // Undefined symbol - might be defined later (time-travel)
            // Mark as needing resolution
            // For now, we'll allow it and let time-travel resolution handle it
        }
    }
}

// Process timing operation
static void process_timing_op(SymbolBuilder* builder, uint16_t node_idx) {
    ASTNode* node = &builder->nodes[node_idx];
    
    // Create temporal scope for operations that affect time
    switch (node->data.timing.timing_op) {
        case TOK_TIMING_ONTO:  // <<
        case TOK_TIMING_INTO:  // >>
            symbol_push_scope(builder->table, true, node->data.timing.temporal_offset);
            break;
    }
    
    // Process the expression
    if (node->data.timing.expr_idx > 0) {
        build_symbols_from_node(builder, node->data.timing.expr_idx);
    }
    
    // Pop temporal scope if we created one
    switch (node->data.timing.timing_op) {
        case TOK_TIMING_ONTO:
        case TOK_TIMING_INTO:
            symbol_pop_scope(builder->table);
            break;
    }
}

// Process action block
static void process_action_block(SymbolBuilder* builder, uint16_t node_idx) {
    ASTNode* node = &builder->nodes[node_idx];
    
    // Create new scope for action block
    symbol_push_scope(builder->table, false, 0);
    
    // Process all actions in the block
    uint16_t action = node->data.binary.left_idx;
    while (action != 0 && action < builder->node_count) {
        build_symbols_from_node(builder, action);
        
        // Get next action in chain
        if (builder->nodes[action].type == NODE_BINARY_OP ||
            builder->nodes[action].type == NODE_EXPRESSION) {
            action = builder->nodes[action].data.binary.right_idx;
        } else {
            break;
        }
    }
    
    // Pop action block scope
    symbol_pop_scope(builder->table);
}

// Process conditional
static void process_conditional(SymbolBuilder* builder, uint16_t node_idx) {
    ASTNode* node = &builder->nodes[node_idx];
    
    // Process the parameter being tested
    uint16_t param_idx = node->data.binary.left_idx;
    if (param_idx > 0) {
        build_symbols_from_node(builder, param_idx);
    }
    
    // Conditionals that use future values need special handling
    // Mark any referenced symbols as temporal
    if (param_idx > 0 && param_idx < builder->node_count) {
        ASTNode* param = &builder->nodes[param_idx];
        if (param->type == NODE_IDENTIFIER) {
            const char* name = &builder->string_pool[param->data.ident.name_offset];
            uint16_t name_len = param->data.ident.name_len;
            
            Symbol* sym = symbol_lookup(builder->table, name, name_len, true);
            if (sym) {
                symbol_mark_temporal(sym);
            }
        }
    }
}

// Main recursive symbol builder
static void build_symbols_from_node(SymbolBuilder* builder, uint16_t node_idx) {
    print_str("[DEBUG] build_symbols_from_node: node_idx=");
    print_num(node_idx);
    if (node_idx < builder->node_count) {
        print_str(" type=");
        print_num(builder->nodes[node_idx].type);
    }
    print_str("\n");
    if (node_idx == 0 || node_idx >= builder->node_count) {
        print_str("build_symbols_from_node: skipping invalid node_idx=");
        print_num(node_idx);
        print_str(" (node_count=");
        print_num(builder->node_count);
        print_str(")\n");
        return;
    }
    if (builder->has_error) return;
    
    ASTNode* node = &builder->nodes[node_idx];
    
    // Remove verbose debug
    
    switch (node->type) {
        case NODE_PROGRAM:
            // Process all top-level statements
            {
                print_str("[SYMBOL] Processing NODE_PROGRAM\n");
                print_str("[SYMBOL] NODE_PROGRAM at node_idx=");
                print_num(node_idx);
                print_str("\n");
                
                // Remove raw bytes debug
                
                print_str("[SYMBOL] node->data.binary.left_idx=");
                print_num(node->data.binary.left_idx);
                print_str(" right_idx=");
                print_num(node->data.binary.right_idx);
                print_str("\n");
                
                uint16_t stmt = node->data.binary.left_idx;
                print_str("[SYMBOL] First statement idx=");
                print_num(stmt);
                print_str(" (should be 2)\n");
                
                // Extra debug
                if (stmt != 2) {
                    print_str("[SYMBOL] ERROR: Expected stmt=2 but got ");
                    print_num(stmt);
                    print_str("\n");
                }
                
                // Bounds check first statement
                if (stmt >= builder->node_count) {
                    print_str("[SYMBOL] ERROR: First statement idx=");
                    print_num(stmt);
                    print_str(" exceeds node_count=");
                    print_num(builder->node_count);
                    print_str("\n");
                    return;
                }
                
                while (stmt != 0 && stmt < builder->node_count) {
                    print_str("[SYMBOL] Processing statement at idx=");
                    print_num(stmt);
                    print_str(" type=");
                    print_num(builder->nodes[stmt].type);
                    print_str("\n");
                    
                    build_symbols_from_node(builder, stmt);
                    
                    ASTNode* stmt_node = &builder->nodes[stmt];
                    stmt = stmt_node->data.binary.right_idx;
                    
                    print_str("[SYMBOL] Next statement idx=");
                    print_num(stmt);
                    
                    // Bounds check next statement
                    if (stmt != 0 && stmt >= builder->node_count) {
                        print_str(" ERROR: exceeds node_count=");
                        print_num(builder->node_count);
                        print_str("\n");
                        return;
                    }
                    print_str("\n");
                }
            }
            break;
            
        case NODE_VAR_DEF:
            process_var_def(builder, node_idx);
            break;
            
        case NODE_FUNC_DEF:
            process_func_def(builder, node_idx);
            break;
            
        case NODE_ARRAY_4D_DEF:
            process_array_4d_def(builder, node_idx);
            break;
            
        case NODE_ARRAY_4D_ACCESS:
            // Process array name and indices
            if (node->data.array_4d.name_idx > 0) {
                build_symbols_from_node(builder, node->data.array_4d.name_idx);
            }
            for (int i = 0; i < 4; i++) {
                if (node->data.array_4d.dim_indices[i] > 0) {
                    build_symbols_from_node(builder, node->data.array_4d.dim_indices[i]);
                }
            }
            break;
            
        case NODE_IDENTIFIER:
            process_identifier(builder, node_idx);
            break;
            
        case NODE_TIMING_OP:
            process_timing_op(builder, node_idx);
            break;
            
        case NODE_ACTION_BLOCK:
            process_action_block(builder, node_idx);
            break;
            
        case NODE_CONDITIONAL:
            process_conditional(builder, node_idx);
            break;
            
        case NODE_BINARY_OP:
            // Process both operands
            build_symbols_from_node(builder, node->data.binary.left_idx);
            build_symbols_from_node(builder, node->data.binary.right_idx);
            break;
            
        case NODE_JUMP:
            // Jump labels are handled separately
            break;
            
        case NODE_NUMBER:
            // Nothing to do for literals
            break;
    }
}

// Main entry point - build symbol table from AST
bool build_symbol_table(SymbolTable* table, ASTNode* nodes, uint16_t root_idx,
                       uint16_t node_count, char* string_pool) {
    
    print_str("[SYMBOL] build_symbol_table called with root_idx=");
    print_num(root_idx);
    print_str(" node_count=");
    print_num(node_count);
    print_str(" nodes addr=");
    print_num((uint64_t)nodes);
    print_str("\n");
    
    // Initialize symbol table
    symbol_table_init(table, string_pool);
    
    // Create builder state
    SymbolBuilder builder = {
        .table = table,
        .nodes = nodes,
        .string_pool = string_pool,
        .node_count = node_count,
        .has_error = false,
        .error_node = 0
    };
    
    print_str("[SYMBOL] Checking nodes[1]: type=");
    print_num(builder.nodes[1].type);
    print_str(" left_idx=");
    print_num(builder.nodes[1].data.binary.left_idx);
    print_str(" right_idx=");
    print_num(builder.nodes[1].data.binary.right_idx);
    print_str("\n");
    
    
    // Build symbols starting from root
    build_symbols_from_node(&builder, root_idx);
    
    
    return !builder.has_error;
}

// Debug print symbol table
void debug_print_symbols(SymbolTable* table) {
    print_str("\n=== SYMBOL TABLE ===\n");
    print_str("Symbols: ");
    print_num(table->symbol_count);
    print_str("\n");
    
    for (uint16_t i = 0; i < table->symbol_count; i++) {
        Symbol* sym = &table->symbols[i];
        
        print_str("  ");
        
        // Print symbol type
        switch (sym->type) {
            case SYMBOL_VARIABLE:  print_str("VAR   "); break;
            case SYMBOL_FUNCTION:  print_str("FUNC  "); break;
            case SYMBOL_ARRAY_4D:  print_str("ARR4D "); break;
            case SYMBOL_TEMPORAL:  print_str("TEMP  "); break;
            case SYMBOL_JUMP_LABEL: print_str("JUMP  "); break;
            default: print_str("???   ");
        }
        
        // Print name
        const char* name = table->string_pool + sym->name_offset;
        for (uint16_t j = 0; j < sym->name_len; j++) {
            char c[2] = {name[j], '\0'};
            print_str(c);
        }
        
        // Print storage info
        print_str(" [");
        switch (sym->storage) {
            case STORAGE_REGISTER:
                print_str("REG ");
                print_num(sym->data.var.reg);
                break;
            case STORAGE_STACK:
                print_str("STACK ");
                print_num(-sym->data.var.stack_offset);
                break;
            case STORAGE_TEMPORAL:
                print_str("TEMP R");
                print_num(sym->data.var.reg);
                break;
            default:
                print_str("???");
        }
        print_str("]");
        
        // Print temporal info
        if (sym->visible_in_past || sym->visible_in_future) {
            print_str(" <time-travel>");
        }
        
        print_str("\n");
    }
    
    print_str("=== END SYMBOLS ===\n");
}