// SYMBOL TABLE - Ultra-fast variable/function tracking
// Stack-allocated, no heap, with temporal scope support

#include "blaze_internals.h"

// Symbol and SymbolTable types are defined in symbol_table_types.h

// Hash function (same as before)
static uint32_t hash_identifier(const char* name, uint16_t len) {
    uint32_t hash = 5381;
    for (uint16_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + name[i];
    }
    return hash;
}

// Initialize symbol table
void symbol_table_init(SymbolTable* table, char* string_pool) {
    // Zero out everything
    for (uint16_t i = 0; i < 128; i++) {
        table->symbols[i].name_hash = 0;
    }
    
    table->symbol_count = 0;
    table->scope_count = 0;
    table->current_scope = 0;
    table->reg_alloc_mask = 0;
    table->temp_reg_mask = 0;
    table->current_stack_offset = 0;
    table->string_pool = string_pool;
    
    // Create global scope
    table->scopes[0].start_index = 0;
    table->scopes[0].symbol_count = 0;
    table->scopes[0].parent_scope = 0;
    table->scopes[0].stack_size = 0;
    table->scope_count = 1;
}

// Push new scope
void symbol_push_scope(SymbolTable* table, bool is_temporal, int32_t temporal_shift) {
    if (table->scope_count >= 64) return; // Scope overflow
    
    uint16_t new_scope = table->scope_count++;
    ScopeFrame* scope = &table->scopes[new_scope];
    
    scope->start_index = table->symbol_count;
    scope->symbol_count = 0;
    scope->parent_scope = table->current_scope;
    scope->stack_size = 0;
    scope->max_stack_offset = table->current_stack_offset;
    scope->is_temporal_scope = is_temporal;
    scope->temporal_shift = temporal_shift;
    
    table->current_scope = new_scope;
}

// Pop scope
void symbol_pop_scope(SymbolTable* table) {
    if (table->current_scope == 0) return; // Can't pop global scope
    
    ScopeFrame* scope = &table->scopes[table->current_scope];
    
    // Free any stack space used by this scope
    table->current_stack_offset = scope->max_stack_offset;
    
    // Free registers used by variables in this scope
    for (uint16_t i = 0; i < scope->symbol_count; i++) {
        Symbol* sym = &table->symbols[scope->start_index + i];
        if (sym->type == SYMBOL_VARIABLE && 
            sym->storage == STORAGE_REGISTER) {
            // Free the register
            uint8_t reg_bit = 1 << sym->data.var.reg;
            table->reg_alloc_mask &= ~reg_bit;
        }
    }
    
    // Return to parent scope
    table->current_scope = scope->parent_scope;
}

// Allocate register for variable
static X64Register allocate_register(SymbolTable* table, bool is_temporal) {
    // Temporal values get R12-R15
    if (is_temporal) {
        for (uint8_t i = 0; i < 4; i++) {
            uint8_t bit = 1 << i;
            if (!(table->temp_reg_mask & bit)) {
                table->temp_reg_mask |= bit;
                return R12 + i;
            }
        }
    }
    
    // Regular values get RBX, R8-R11
    X64Register candidates[] = {RBX, R8, R9, R10, R11};
    for (uint8_t i = 0; i < 5; i++) {
        uint8_t bit = 1 << candidates[i];
        if (!(table->reg_alloc_mask & bit)) {
            table->reg_alloc_mask |= bit;
            return candidates[i];
        }
    }
    
    return RAX; // Fallback to accumulator
}

// Add variable to symbol table
Symbol* symbol_add_variable(SymbolTable* table, const char* name, uint16_t name_len,
                           bool is_temporal, bool is_mutable) {
    if (table->symbol_count >= 512) return NULL; // Symbol table full
    
    Symbol* sym = &table->symbols[table->symbol_count++];
    
    // Basic info
    sym->name_hash = hash_identifier(name, name_len);
    sym->name_offset = name - table->string_pool; // Calculate offset
    sym->name_len = name_len;
    sym->type = is_temporal ? SYMBOL_TEMPORAL : SYMBOL_VARIABLE;
    
    // Scope info
    sym->scope_level = table->current_scope;
    sym->declaration_line = 0; // Could track line numbers
    
    // Variable data
    sym->data.var.is_mutable = is_mutable;
    sym->data.var.is_temporal = is_temporal;
    
    // Try to allocate register
    X64Register reg = allocate_register(table, is_temporal);
    if (reg != RAX) {
        sym->storage = STORAGE_REGISTER;
        sym->data.var.reg = reg;
    } else {
        // Allocate stack space
        sym->storage = STORAGE_STACK;
        table->current_stack_offset -= 8; // 64-bit values
        sym->data.var.stack_offset = table->current_stack_offset;
    }
    
    // Temporal visibility
    if (is_temporal) {
        sym->visible_in_past = true;    // Temporal vars visible everywhere
        sym->visible_in_future = true;
        sym->temporal_offset = table->scopes[table->current_scope].temporal_shift;
    } else {
        sym->visible_in_past = false;
        sym->visible_in_future = false;
        sym->temporal_offset = 0;
    }
    
    // Update scope
    table->scopes[table->current_scope].symbol_count++;
    
    return sym;
}

// Add function to symbol table
Symbol* symbol_add_function(SymbolTable* table, const char* name, uint16_t name_len,
                           uint16_t ast_node, uint8_t param_count) {
    if (table->symbol_count >= 512) return NULL;
    
    Symbol* sym = &table->symbols[table->symbol_count++];
    
    sym->name_hash = hash_identifier(name, name_len);
    sym->name_offset = name - table->string_pool;
    sym->name_len = name_len;
    sym->type = SYMBOL_FUNCTION;
    sym->storage = STORAGE_GLOBAL; // Functions are global
    
    sym->data.func.ast_node = ast_node;
    sym->data.func.param_count = param_count;
    sym->data.func.return_count = 1; // Default
    sym->data.func.has_temporal_deps = false;
    sym->data.func.code_offset = 0;
    
    sym->scope_level = table->current_scope;
    sym->visible_in_past = false;
    sym->visible_in_future = true; // Functions visible after declaration
    
    table->scopes[table->current_scope].symbol_count++;
    
    return sym;
}

// Add 4D array to symbol table
Symbol* symbol_add_array_4d(SymbolTable* table, const char* name, 
                           uint32_t x, uint32_t y, uint32_t z, uint32_t t) {
    uint16_t name_len = 0;
    while (name[name_len]) name_len++;
    if (table->symbol_count >= 512) return NULL;
    
    Symbol* sym = &table->symbols[table->symbol_count++];
    
    sym->name_hash = hash_identifier(name, name_len);
    sym->name_offset = name - table->string_pool;
    sym->name_len = name_len;
    sym->type = SYMBOL_ARRAY_4D;
    sym->storage = STORAGE_STACK; // Arrays on stack for now
    
    // Copy dimensions
    sym->data.array_4d.dimensions[0] = x;
    sym->data.array_4d.dimensions[1] = y;
    sym->data.array_4d.dimensions[2] = z;
    sym->data.array_4d.dimensions[3] = t;
    
    // Calculate size and allocate with overflow check
    // Check for overflow when multiplying dimensions
    uint64_t total_elements = (uint64_t)x * y * z * t;
    if (total_elements > (1ULL << 29)) { // Max 512M elements
        return NULL; // Array too large
    }
    
    uint64_t total_size = total_elements * 8;
    if (total_size > 0xFFFFFFFF) { // Max 4GB
        return NULL; // Array too large
    }
    
    table->current_stack_offset -= (uint32_t)total_size;
    sym->data.array_4d.base_addr = table->current_stack_offset;
    sym->data.array_4d.is_temporal_indexed = true; // 4th dimension is time
    
    sym->scope_level = table->current_scope;
    table->scopes[table->current_scope].symbol_count++;
    
    return sym;
}

// Lookup symbol (with temporal awareness)
Symbol* symbol_lookup(SymbolTable* table, const char* name, uint16_t name_len,
                     bool from_future) {
    uint32_t hash = hash_identifier(name, name_len);
    
    // Search from current scope outward
    uint16_t scope = table->current_scope;
    
    while (true) {
        ScopeFrame* frame = &table->scopes[scope];
        
        // Search symbols in this scope
        for (uint16_t i = 0; i < frame->symbol_count; i++) {
            Symbol* sym = &table->symbols[frame->start_index + i];
            
            if (sym->name_hash == hash && sym->name_len == name_len) {
                // Check temporal visibility
                if (from_future && !sym->visible_in_future) {
                    continue; // Not visible from future
                }
                
                // Verify exact name match
                const char* sym_name = table->string_pool + sym->name_offset;
                bool match = true;
                for (uint16_t j = 0; j < name_len; j++) {
                    if (sym_name[j] != name[j]) {
                        match = false;
                        break;
                    }
                }
                
                if (match) return sym;
            }
        }
        
        // Move to parent scope
        if (scope == 0) break; // Global scope
        scope = frame->parent_scope;
    }
    
    return NULL; // Not found
}

// Lookup with temporal offset (for time-travel)
Symbol* symbol_lookup_temporal(SymbolTable* table, const char* name, uint16_t name_len,
                              int32_t temporal_offset) {
    uint32_t hash = hash_identifier(name, name_len);
    
    // Search all scopes for temporal matches
    for (uint16_t i = 0; i < table->symbol_count; i++) {
        Symbol* sym = &table->symbols[i];
        
        if (sym->name_hash == hash && sym->name_len == name_len) {
            // Check temporal visibility
            if (sym->visible_in_past || sym->visible_in_future ||
                sym->temporal_offset == temporal_offset) {
                
                const char* sym_name = table->string_pool + sym->name_offset;
                bool match = true;
                for (uint16_t j = 0; j < name_len; j++) {
                    if (sym_name[j] != name[j]) {
                        match = false;
                        break;
                    }
                }
                
                if (match) return sym;
            }
        }
    }
    
    return NULL;
}

// Mark symbol as having temporal dependencies
void symbol_mark_temporal(Symbol* sym) {
    if (sym->type == SYMBOL_FUNCTION) {
        sym->data.func.has_temporal_deps = true;
    } else if (sym->type == SYMBOL_VARIABLE) {
        sym->data.var.is_temporal = true;
        sym->visible_in_past = true;
        sym->visible_in_future = true;
    }
}

// Get storage info for code generation
void symbol_get_storage(Symbol* sym, X64Register* reg, int32_t* offset) {
    switch (sym->storage) {
        case STORAGE_REGISTER:
        case STORAGE_TEMPORAL:
            *reg = sym->data.var.reg;
            *offset = 0;
            break;
            
        case STORAGE_STACK:
            *reg = RBP;
            *offset = sym->type == SYMBOL_ARRAY_4D ? 
                     sym->data.array_4d.base_addr :
                     sym->data.var.stack_offset;
            break;
            
        case STORAGE_IMMEDIATE:
            *reg = RAX; // Will load immediate
            *offset = sym->data.var.value;
            break;
            
        default:
            *reg = RAX;
            *offset = 0;
    }
}