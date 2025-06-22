// RUNTIME MEMORY EXECUTION ENGINE
// Manages temporal memory during Blaze program execution

#include "blaze_internals.h"

// Forward declarations from memory_temporal.c
extern void temporal_memory_init(void* stack_base, uint32_t stack_size);
// temporal_alloc_var and temporal_create_link are declared in blaze_internals.h
extern void* temporal_resolve_var(const char* name, bool needs_future_value);
extern Array4D* temporal_alloc_array4d(uint32_t x, uint32_t y, uint32_t z, uint32_t t,
                                      uint32_t elem_size);
extern void* temporal_array4d_access(Array4D* arr, uint32_t x, uint32_t y, uint32_t z, uint32_t t);
extern void temporal_memory_stats(uint32_t* past_used, uint32_t* present_used, 
                                 uint32_t* future_used, uint16_t* link_count);
extern void temporal_memory_stats_extended(uint32_t* past_used, uint32_t* present_used, 
                                          uint32_t* future_used, uint32_t* unknown_used,
                                          uint16_t* link_count, uint16_t* gap_count);
extern void* temporal_alloc_gap_var(const char* name, uint32_t size, float initial_confidence,
                                   float migration_threshold);
extern void temporal_gap_add_missing(const char* var_name, const char* missing_item, bool is_critical);
extern void temporal_gap_update_confidence(const char* var_name, float new_confidence);
// GapMetadata is defined in blaze_types.h
extern GapMetadata* temporal_get_gap_metadata(const char* var_name);
extern bool temporal_gap_migrate(const char* var_name);

// TemporalLink structure (from memory_temporal.c)
struct TemporalLink {
    uint32_t var_hash;
    void* past_addr;
    void* present_addr;
    void* future_addr;
    uint16_t link_count;
    int32_t temporal_offset;
    struct TemporalLink* next;
};
typedef struct TemporalLink TemporalLink;

// TimeZone is defined in blaze_types.h

// Hash function
static uint32_t hash_var_name(const char* name) {
    uint32_t hash = 5381;
    while (*name) {
        hash = ((hash << 5) + hash) + *name++;
    }
    return hash;
}

// Runtime value types
typedef enum {
    VALUE_INT64,
    VALUE_FLOAT64,
    VALUE_POINTER,
    VALUE_TEMPORAL_REF,
    VALUE_ARRAY_4D
} ValueType;

// Runtime value structure
struct RuntimeValueStruct {
    ValueType type;
    union {
        int64_t int_val;
        double float_val;
        void* ptr_val;
        struct {
            void* addr;
            int32_t temporal_offset;
        } temporal_ref;
        Array4D* array_4d;
    } data;
};

// Execution frame with temporal context
typedef struct ExecutionFrame {
    // Stack values
    RuntimeValue* stack;
    uint32_t stack_size;
    uint32_t stack_top;
    
    // Temporal context
    TemporalMemory* temporal_mem;
    int32_t temporal_offset;    // Current time offset
    bool in_future_context;     // Executing future code
    
    // Local variables
    struct {
        uint32_t name_hash;
        void* addr;
        ValueType type;
        TimeZone zone;
    } locals[64];
    uint16_t local_count;
    
    // Parent frame (for nested scopes)
    struct ExecutionFrame* parent;
} ExecutionFrame;

// Global execution context
static struct {
    ExecutionFrame* current_frame;
    uint8_t* memory_pool;       // Pre-allocated memory pool
    uint32_t pool_size;
    uint32_t pool_used;
} g_runtime;

// Initialize runtime system
void runtime_init(uint32_t memory_size) {
    // Allocate memory pool using mmap
    g_runtime.memory_pool = (uint8_t*)syscall6(SYS_MMAP, 0, memory_size, 
                                              0x3, 0x22, -1, 0);
    g_runtime.pool_size = memory_size;
    g_runtime.pool_used = 0;
    g_runtime.current_frame = NULL;
    
    // Initialize temporal memory system
    temporal_memory_init(g_runtime.memory_pool, memory_size / 2);
}

// Create new execution frame
ExecutionFrame* runtime_push_frame(bool is_temporal, int32_t temporal_offset) {
    // Allocate frame from pool
    if (g_runtime.pool_used + sizeof(ExecutionFrame) > g_runtime.pool_size) {
        return NULL; // Out of memory
    }
    
    ExecutionFrame* frame = (ExecutionFrame*)(g_runtime.memory_pool + g_runtime.pool_used);
    g_runtime.pool_used += sizeof(ExecutionFrame);
    
    // Initialize frame
    frame->stack_size = 256; // 256 values
    frame->stack = (RuntimeValue*)(g_runtime.memory_pool + g_runtime.pool_used);
    g_runtime.pool_used += sizeof(RuntimeValue) * frame->stack_size;
    frame->stack_top = 0;
    
    frame->temporal_offset = temporal_offset;
    frame->in_future_context = is_temporal;
    frame->local_count = 0;
    frame->parent = g_runtime.current_frame;
    
    g_runtime.current_frame = frame;
    
    return frame;
}

// Pop execution frame
void runtime_pop_frame(void) {
    if (g_runtime.current_frame) {
        g_runtime.current_frame = g_runtime.current_frame->parent;
    }
}

// Allocate local variable with temporal awareness
void* runtime_alloc_local(const char* name, uint32_t size, bool is_temporal) {
    ExecutionFrame* frame = g_runtime.current_frame;
    if (!frame || frame->local_count >= 64) return NULL;
    
    // Determine time zone based on context
    TimeZone zone = ZONE_PRESENT;
    if (is_temporal) {
        zone = frame->in_future_context ? ZONE_FUTURE : ZONE_PAST;
    }
    
    // Allocate through temporal memory system
    void* addr = temporal_alloc_var(name, size, zone);
    if (!addr) return NULL;
    
    // Register in frame
    uint32_t idx = frame->local_count++;
    frame->locals[idx].name_hash = hash_var_name(name);
    frame->locals[idx].addr = addr;
    frame->locals[idx].type = VALUE_INT64; // Default
    frame->locals[idx].zone = zone;
    
    return addr;
}

// Store value with temporal tracking
void runtime_store_value(const char* name, RuntimeValue* value, bool to_future) {
    ExecutionFrame* frame = g_runtime.current_frame;
    if (!frame) return;
    
    // Find or allocate variable
    uint32_t name_hash = hash_var_name(name);
    void* addr = NULL;
    
    for (uint16_t i = 0; i < frame->local_count; i++) {
        if (frame->locals[i].name_hash == name_hash) {
            addr = frame->locals[i].addr;
            break;
        }
    }
    
    if (!addr) {
        // Allocate new variable
        addr = runtime_alloc_local(name, 8, to_future);
    }
    
    if (!addr) return;
    
    // Store value based on type
    switch (value->type) {
        case VALUE_INT64:
            *(int64_t*)addr = value->data.int_val;
            break;
            
        case VALUE_FLOAT64:
            *(double*)addr = value->data.float_val;
            break;
            
        case VALUE_POINTER:
            *(void**)addr = value->data.ptr_val;
            break;
            
        case VALUE_TEMPORAL_REF:
            // Create temporal link
            temporal_create_link(name, ZONE_PRESENT, 
                               to_future ? ZONE_FUTURE : ZONE_PAST,
                               value->data.temporal_ref.temporal_offset);
            *(void**)addr = value->data.temporal_ref.addr;
            break;
    }
    
    // If storing to future, create temporal link
    if (to_future) {
        temporal_create_link(name, ZONE_PRESENT, ZONE_FUTURE, 
                           frame->temporal_offset);
    }
}

// Load value with temporal resolution
RuntimeValue runtime_load_value(const char* name, bool from_future) {
    RuntimeValue result = {0};
    ExecutionFrame* frame = g_runtime.current_frame;
    if (!frame) return result;
    
    // Try temporal resolution first
    void* addr = temporal_resolve_var(name, from_future);
    
    if (!addr) {
        // Search in current frame
        uint32_t name_hash = hash_var_name(name);
        for (uint16_t i = 0; i < frame->local_count; i++) {
            if (frame->locals[i].name_hash == name_hash) {
                addr = frame->locals[i].addr;
                result.type = frame->locals[i].type;
                break;
            }
        }
    }
    
    if (addr) {
        // Load based on type
        switch (result.type) {
            case VALUE_INT64:
                result.data.int_val = *(int64_t*)addr;
                break;
                
            case VALUE_FLOAT64:
                result.data.float_val = *(double*)addr;
                break;
                
            case VALUE_POINTER:
            case VALUE_TEMPORAL_REF:
                result.data.ptr_val = *(void**)addr;
                break;
        }
    }
    
    return result;
}

// Push value onto execution stack
void runtime_push(RuntimeValue* value) {
    ExecutionFrame* frame = g_runtime.current_frame;
    if (!frame || frame->stack_top >= frame->stack_size) return;
    
    frame->stack[frame->stack_top++] = *value;
}

// Pop value from execution stack
RuntimeValue runtime_pop(void) {
    RuntimeValue result = {0};
    ExecutionFrame* frame = g_runtime.current_frame;
    
    if (frame && frame->stack_top > 0) {
        result = frame->stack[--frame->stack_top];
    }
    
    return result;
}

// Execute temporal operation
void runtime_temporal_op(TokenType op, const char* var_name) {
    ExecutionFrame* frame = g_runtime.current_frame;
    if (!frame) return;
    
    switch (op) {
        case TOK_TIMING_INTO: // >> - Store to future
            {
                RuntimeValue val = runtime_pop();
                runtime_store_value(var_name, &val, true);
            }
            break;
            
        case TOK_LT: // < - Load from past/future
            {
                RuntimeValue val = runtime_load_value(var_name, true);
                runtime_push(&val);
            }
            break;
            
        case TOK_TIMING_ONTO: // << - Strong backward reference
            {
                // Create strong temporal link
                temporal_create_link(var_name, ZONE_FUTURE, ZONE_PAST, -1);
            }
            break;
            
        case TOK_TIMING_BOTH: // <> - Bidirectional
            {
                // Create bidirectional link
                temporal_create_link(var_name, ZONE_PAST, ZONE_FUTURE, 0);
                temporal_create_link(var_name, ZONE_FUTURE, ZONE_PAST, 0);
            }
            break;
    }
}

// Allocate and access 4D arrays
RuntimeValue runtime_alloc_array4d(uint32_t x, uint32_t y, uint32_t z, uint32_t t) {
    RuntimeValue result = {0};
    
    Array4D* arr = temporal_alloc_array4d(x, y, z, t, sizeof(double));
    if (arr) {
        result.type = VALUE_ARRAY_4D;
        result.data.array_4d = arr;
    }
    
    return result;
}

// Access 4D array element
RuntimeValue runtime_array4d_get(Array4D* arr, uint32_t x, uint32_t y, 
                                uint32_t z, uint32_t t) {
    RuntimeValue result = {0};
    
    void* elem_addr = temporal_array4d_access(arr, x, y, z, t);
    if (elem_addr) {
        result.type = VALUE_FLOAT64;
        result.data.float_val = *(double*)elem_addr;
    }
    
    return result;
}

// Set 4D array element
void runtime_array4d_set(Array4D* arr, uint32_t x, uint32_t y, uint32_t z, 
                        uint32_t t, double value) {
    void* elem_addr = temporal_array4d_access(arr, x, y, z, t);
    if (elem_addr) {
        *(double*)elem_addr = value;
    }
}

// GAP analysis for runtime memory usage
void runtime_gap_analysis(MemoryPrediction* pred) {
    // Get current memory stats
    uint32_t past_used, present_used, future_used;
    uint16_t link_count;
    
    temporal_memory_stats(&past_used, &present_used, &future_used, &link_count);
    
    // Analyze gaps
    pred->stack_usage = past_used + present_used + future_used;
    pred->temporal_links = link_count * sizeof(TemporalLink);
    pred->future_zone_usage = future_used;
    
    // Check against limits
    pred->will_overflow = (pred->stack_usage > g_runtime.pool_size * 0.8);
}

// Allocate GAP variable
RuntimeValue runtime_alloc_gap_var(const char* name, float initial_confidence) {
    RuntimeValue result = {0};
    ExecutionFrame* frame = g_runtime.current_frame;
    if (!frame) return result;
    
    // Allocate in unknown zone with GAP metadata
    void* addr = temporal_alloc_gap_var(name, sizeof(double), initial_confidence, 0.7);
    if (!addr) return result;
    
    // Register in frame
    if (frame->local_count < 64) {
        uint32_t idx = frame->local_count++;
        frame->locals[idx].name_hash = hash_var_name(name);
        frame->locals[idx].addr = addr;
        frame->locals[idx].type = VALUE_FLOAT64; // Default to float
        frame->locals[idx].zone = ZONE_UNKNOWN;
    }
    
    result.type = VALUE_POINTER;
    result.data.ptr_val = addr;
    
    return result;
}

// Update GAP variable confidence
void runtime_gap_update_confidence(const char* name, float confidence) {
    temporal_gap_update_confidence(name, confidence);
    
    // Check if migration needed
    GapMetadata* meta = temporal_get_gap_metadata(name);
    if (meta && confidence >= meta->migration_threshold) {
        // Migrate from unknown to target zone
        if (temporal_gap_migrate(name)) {
            // Update frame local info
            ExecutionFrame* frame = g_runtime.current_frame;
            if (frame) {
                uint32_t name_hash = hash_var_name(name);
                for (uint16_t i = 0; i < frame->local_count; i++) {
                    if (frame->locals[i].name_hash == name_hash) {
                        frame->locals[i].zone = meta->target_zone;
                        break;
                    }
                }
            }
        }
    }
}

// Add missing data to GAP variable
void runtime_gap_add_missing(const char* var_name, const char* missing_item) {
    temporal_gap_add_missing(var_name, missing_item, true); // Mark as critical
}

// Execute GAP computation
RuntimeValue runtime_gap_compute(const char* var_name) {
    RuntimeValue result = {0};
    
    // Get GAP metadata
    GapMetadata* meta = temporal_get_gap_metadata(var_name);
    if (!meta) return result;
    
    // Check if computation is needed
    if (meta->confidence_score >= meta->migration_threshold) {
        // Already computed with sufficient confidence
        void* addr = temporal_resolve_var(var_name, false);
        if (addr) {
            result.type = VALUE_FLOAT64;
            result.data.float_val = *(double*)addr;
        }
        return result;
    }
    
    // Trigger computation based on available data
    // This is where the actual GAP algorithm would run
    // For now, return provisional result
    result.type = VALUE_FLOAT64;
    result.data.float_val = 0.5; // Default provisional value
    
    return result;
}

// Debug print memory layout
void runtime_debug_memory(void) {
    uint32_t past_used, present_used, future_used, unknown_used;
    uint16_t link_count, gap_count;
    
    // Use extended stats if available
    temporal_memory_stats_extended(&past_used, &present_used, &future_used, 
                                  &unknown_used, &link_count, &gap_count);
    
    print_str("\n=== TEMPORAL MEMORY LAYOUT ===\n");
    print_str("FUTURE ZONE:  ");
    print_num(future_used);
    print_str(" bytes\n");
    
    print_str("UNKNOWN ZONE: ");
    print_num(unknown_used);
    print_str(" bytes (GAP)\n");
    
    print_str("PRESENT ZONE: ");
    print_num(present_used);
    print_str(" bytes\n");
    
    print_str("PAST ZONE:    ");
    print_num(past_used);
    print_str(" bytes\n");
    
    print_str("TEMPORAL LINKS: ");
    print_num(link_count);
    print_str("\n");
    
    print_str("GAP VARIABLES: ");
    print_num(gap_count);
    print_str("\n");
    
    print_str("=== END MEMORY LAYOUT ===\n");
}