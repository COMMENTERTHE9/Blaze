// TIME-TRAVEL RESOLUTION ENGINE
// Second pass that links temporal dependencies and resolves execution order

#include "blaze_internals.h"

// Temporal dependency types
typedef enum {
    TEMPORAL_BACKWARD_VALUE,    // Value flows from future to past
    TEMPORAL_FORWARD_VALUE,     // Normal forward flow
    TEMPORAL_BOTH_WAYS,         // Bidirectional flow (<>)
    TEMPORAL_PARALLEL           // Parallel temporal execution
} TemporalLinkType;

// Temporal dependency link
typedef struct TimeLink {
    uint16_t past_consumer_idx;     // Node that needs future value
    uint16_t future_creator_idx;    // Node that creates the value
    TemporalLinkType link_type;
    uint32_t identifier_hash;       // Hash of the identifier being linked
    struct TimeLink* next;
} TimeLink;

// Execution step structure now defined in header
struct ExecutionStep {
    uint16_t node_idx;
    int32_t temporal_order;         // Negative = execute early, Positive = execute late
    bool requires_future_value;
    bool creates_past_value;
    uint32_t dependencies[8];       // Up to 8 temporal dependencies
    uint8_t dep_count;
};

// Temporal resolution state
typedef struct {
    ASTNode* nodes;
    uint16_t node_count;
    char* string_pool;
    
    // Temporal links
    TimeLink* links;
    uint32_t link_count;
    
    // Execution plan
    ExecutionStep* steps;
    uint32_t step_count;
    
    // Symbol tracking
    uint32_t future_values[64];     // Hash table of future-created values
    uint32_t future_count;
} TemporalResolver;

// Simple string hash for identifier matching
static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + *str++;
    }
    return hash;
}

// Find nodes that create a value in the future
static uint16_t find_future_creator(TemporalResolver* resolver, const char* identifier, 
                                   uint16_t start_after) {
    uint32_t target_hash = hash_string(identifier);
    
    // Search forward from current position
    for (uint16_t i = start_after + 1; i < resolver->node_count; i++) {
        ASTNode* node = &resolver->nodes[i];
        
        // Check for store operations with timing operators
        if (node->type == NODE_TIMING_OP) {
            // Check if this is >> (INTO) creating a value
            if (node->data.timing.timing_op == TOK_TIMING_INTO) {
                // Check if the expression creates our identifier
                uint16_t expr_idx = node->data.timing.expr_idx;
                if (expr_idx > 0 && expr_idx < resolver->node_count) {
                    ASTNode* expr = &resolver->nodes[expr_idx];
                    if (expr->type == NODE_IDENTIFIER) {
                        const char* name = &resolver->string_pool[expr->data.ident.name_offset];
                        if (hash_string(name) == target_hash) {
                            return i; // Found the creator!
                        }
                    }
                }
            }
        }
        
        // Check for variable assignments that flow backward
        if (node->type == NODE_BINARY_OP && node->data.binary.op == TOK_GT) {
            // Check right side for our identifier
            uint16_t right_idx = node->data.binary.right_idx;
            if (right_idx > 0 && right_idx < resolver->node_count) {
                ASTNode* right = &resolver->nodes[right_idx];
                if (right->type == NODE_IDENTIFIER) {
                    const char* name = &resolver->string_pool[right->data.ident.name_offset];
                    if (hash_string(name) == target_hash) {
                        return i; // Found assignment
                    }
                }
            }
        }
    }
    
    return 0; // Not found
}

// Scan for temporal patterns in AST
static void scan_temporal_patterns(TemporalResolver* resolver, uint16_t node_idx) {
    if (node_idx == 0 || node_idx >= resolver->node_count) return;
    
    ASTNode* node = &resolver->nodes[node_idx];
    
    // Pattern 1: Conditional using future value
    if (node->type == NODE_CONDITIONAL) {
        // Get the parameter being tested
        uint16_t param_idx = node->data.binary.left_idx;
        if (param_idx > 0 && param_idx < resolver->node_count) {
            ASTNode* param = &resolver->nodes[param_idx];
            if (param->type == NODE_IDENTIFIER) {
                const char* param_name = &resolver->string_pool[param->data.ident.name_offset];
                
                // Find where this value is created in the future
                uint16_t creator_idx = find_future_creator(resolver, param_name, node_idx);
                if (creator_idx > 0) {
                    // Add temporal link
                    TimeLink* link = (TimeLink*)((char*)resolver->links + 
                                                 resolver->link_count * sizeof(TimeLink));
                    link->past_consumer_idx = node_idx;
                    link->future_creator_idx = creator_idx;
                    link->link_type = TEMPORAL_BACKWARD_VALUE;
                    link->identifier_hash = hash_string(param_name);
                    resolver->link_count++;
                }
            }
        }
    }
    
    // Pattern 2: Time-travel operators creating dependencies
    if (node->type == NODE_TIMING_OP) {
        switch (node->data.timing.timing_op) {
            case TOK_LT:  // < (BEFORE) - backward reference
            case TOK_BEFORE:  // < (BEFORE) - backward reference
                // This consumes a value from the past
                break;
                
            case TOK_TIMING_ONTO:  // << (ONTO) - strong backward flow
            case TOK_ONTO:  // << (ONTO) - strong backward flow
                // Creates strong temporal dependency
                if (node->data.timing.temporal_offset < 0) {
                    // Mark for early execution
                    ExecutionStep* step = &resolver->steps[resolver->step_count++];
                    step->node_idx = node_idx;
                    step->temporal_order = -2; // Execute very early
                    step->creates_past_value = true;
                }
                break;
                
            case TOK_TIMING_INTO:  // >> (INTO) - forward projection
            case TOK_INTO:  // >> (INTO) - forward projection
                // This creates a future value
                ExecutionStep* step = &resolver->steps[resolver->step_count++];
                step->node_idx = node_idx;
                step->temporal_order = -1; // Execute early to provide future values
                step->creates_past_value = true;
                break;
                
            case TOK_TIMING_BOTH:  // <> (BOTH) - bidirectional
            case TOK_BOTH:  // <> (BOTH) - bidirectional
                // Can both consume and create temporal values
                TimeLink* link = (TimeLink*)((char*)resolver->links + 
                                           resolver->link_count * sizeof(TimeLink));
                link->past_consumer_idx = node_idx;
                link->future_creator_idx = node_idx;
                link->link_type = TEMPORAL_BOTH_WAYS;
                resolver->link_count++;
                break;
                
            case TOK_GT:  // > (AFTER) - forward reference
            case TOK_AFTER:  // > (AFTER) - forward reference
                // This creates a value for the future
                break;
                
            default:
                // Other timing operations don't create temporal dependencies
                break;
        }
    }
    
    // Pattern 3: Jump markers with temporal flow
    if (node->type == NODE_JUMP) {
        // Jumps can create temporal loops
        // Mark for special handling
        ExecutionStep* step = &resolver->steps[resolver->step_count++];
        step->node_idx = node_idx;
        step->temporal_order = 0; // Normal order but tracked
    }
    
    // Recursively scan based on node type
    switch (node->type) {
        case NODE_PROGRAM:
        case NODE_ACTION_BLOCK:
            // Scan all statements/actions
            {
                uint16_t stmt = node->data.binary.left_idx;
                while (stmt != 0 && stmt < resolver->node_count) {
                    scan_temporal_patterns(resolver, stmt);
                    // Get next in chain
                    ASTNode* stmt_node = &resolver->nodes[stmt];
                    
                    // Only access binary.right_idx if node supports it
                    if (stmt_node->type == NODE_PROGRAM || 
                        stmt_node->type == NODE_ACTION_BLOCK ||
                        stmt_node->type == NODE_BINARY_OP ||
                        stmt_node->type == NODE_VAR_DEF ||
                        stmt_node->type == NODE_OUTPUT ||
                        stmt_node->type == NODE_CONDITIONAL) {
                        stmt = stmt_node->data.binary.right_idx;
                    } else {
                        // Node doesn't have chaining, exit loop
                        break;
                    }
                }
            }
            break;
            
        case NODE_BINARY_OP:
            if (node->data.binary.left_idx < resolver->node_count) {
                scan_temporal_patterns(resolver, node->data.binary.left_idx);
            }
            if (node->data.binary.right_idx < resolver->node_count) {
                scan_temporal_patterns(resolver, node->data.binary.right_idx);
            }
            break;
            
        case NODE_TIMING_OP:
            if (node->data.timing.expr_idx < resolver->node_count) {
                scan_temporal_patterns(resolver, node->data.timing.expr_idx);
            }
            break;
            
        case NODE_VAR_DEF:
            // Check initializer
            {
                uint16_t init_idx = node->data.ident.name_len >> 16;
                if (init_idx > 0 && init_idx < resolver->node_count) {
                    scan_temporal_patterns(resolver, init_idx);
                }
            }
            break;
            
        default:
            // Other node types don't need temporal scanning
            break;
    }
}

// Build execution plan with temporal ordering
static void build_execution_plan(TemporalResolver* resolver) {
    // Phase 1: Mark all nodes with default execution order
    // Only process nodes that actually exist and fit in our buffer
    uint16_t max_steps = 512; // Size of step_buffer
    uint16_t actual_nodes = resolver->node_count < max_steps ? resolver->node_count : max_steps;
    
    for (uint16_t i = 0; i < actual_nodes; i++) {
        if (resolver->steps[i].node_idx == 0) {
            resolver->steps[i].node_idx = i;
            resolver->steps[i].temporal_order = i; // Default: sequential
        }
    }
    
    // Phase 2: Apply temporal links to adjust execution order
    for (uint32_t i = 0; i < resolver->link_count; i++) {
        TimeLink* link = (TimeLink*)((char*)resolver->links + i * sizeof(TimeLink));
        
        if (link->link_type == TEMPORAL_BACKWARD_VALUE) {
            // Future creator must execute before past consumer
            uint16_t creator_idx = link->future_creator_idx;
            uint16_t consumer_idx = link->past_consumer_idx;
            
            // Find steps
            for (uint32_t j = 0; j < resolver->step_count; j++) {
                if (resolver->steps[j].node_idx == creator_idx) {
                    resolver->steps[j].temporal_order = -10; // Execute very early
                    resolver->steps[j].creates_past_value = true;
                }
                if (resolver->steps[j].node_idx == consumer_idx) {
                    resolver->steps[j].requires_future_value = true;
                    // Add dependency
                    if (resolver->steps[j].dep_count < 8) {
                        resolver->steps[j].dependencies[resolver->steps[j].dep_count++] = 
                            link->identifier_hash;
                    }
                }
            }
        }
    }
    
    // Phase 3: Sort execution steps by temporal order
    // Simple bubble sort (good enough for small programs)
    for (uint32_t i = 0; i < resolver->step_count; i++) {
        for (uint32_t j = i + 1; j < resolver->step_count; j++) {
            if (resolver->steps[j].temporal_order < resolver->steps[i].temporal_order) {
                // Swap
                ExecutionStep temp = resolver->steps[i];
                resolver->steps[i] = resolver->steps[j];
                resolver->steps[j] = temp;
            }
        }
    }
}

// Validate temporal consistency (no paradoxes)
static bool validate_temporal_consistency(TemporalResolver* resolver) {
    // Check for circular dependencies
    for (uint32_t i = 0; i < resolver->link_count; i++) {
        TimeLink* link = (TimeLink*)((char*)resolver->links + i * sizeof(TimeLink));
        
        // Simple check: A node cannot depend on itself
        if (link->past_consumer_idx == link->future_creator_idx &&
            link->link_type != TEMPORAL_BOTH_WAYS) {
            return false; // Temporal paradox!
        }
    }
    
    // Check for impossible temporal flows
    // (More complex validation could be added here)
    
    return true;
}

// Main time-travel resolution function
bool resolve_time_travel(ASTNode* nodes, uint16_t root_idx, uint16_t node_count, 
                        char* string_pool, ExecutionStep* execution_plan) {
    // Allocate resolver state (stack-based)
    static char link_buffer[sizeof(TimeLink) * 256];
    static char step_buffer[sizeof(ExecutionStep) * 512];
    
    TemporalResolver resolver = {
        .nodes = nodes,
        .node_count = node_count,
        .string_pool = string_pool,
        .links = (TimeLink*)link_buffer,
        .link_count = 0,
        .steps = (ExecutionStep*)step_buffer,
        .step_count = 0,
        .future_count = 0
    };
    
    // Step 1: Scan for temporal patterns
    scan_temporal_patterns(&resolver, root_idx);
    
    // Step 2: Build execution plan
    build_execution_plan(&resolver);
    
    // Step 3: Validate consistency
    if (!validate_temporal_consistency(&resolver)) {
        return false; // Temporal paradox detected!
    }
    
    // Step 4: Copy execution plan to output
    for (uint32_t i = 0; i < resolver.step_count && i < 512; i++) {
        execution_plan[i] = resolver.steps[i];
    }
    
    return true;
}

// Helper to check if a node creates temporal values
bool creates_temporal_value(ASTNode* node) {
    if (node->type == NODE_TIMING_OP) {
        return node->data.timing.timing_op == TOK_TIMING_INTO ||
               node->data.timing.timing_op == TOK_TIMING_BOTH;
    }
    return false;
}

// Helper to check if a node consumes future values
bool consumes_future_value(ASTNode* node) {
    if (node->type == NODE_CONDITIONAL) {
        return true; // Conditionals often use future values
    }
    if (node->type == NODE_TIMING_OP) {
        return node->data.timing.timing_op == TOK_LT ||
               node->data.timing.timing_op == TOK_TIMING_ONTO;
    }
    return false;
}