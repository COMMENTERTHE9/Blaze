// BLAZE MEMORY MANAGER - Competent Three-Tier System
// Arena + Reference Counting + Time Travel Zones + GGGX Support

#include "blaze_internals.h"

// Memory Layout:
// 0x100000-0x700000: Arena pool (6MB) - Fast temporary allocations
// 0x700000-0xA00000: Time travel zones (3MB) - Past/Present/Future
// 0xA00000-0x2000000: Ref-counted heap (22MB) - Long-lived objects
// 0x2000000-0x3000000: GGGX metadata (16MB) - Computational traces

#define ARENA_START    0x100000
#define ARENA_SIZE     0x600000   // 6MB
#define TEMPORAL_START 0x700000
#define ZONE_SIZE      0x100000   // 1MB per zone
#define HEAP_START     0xA00000
#define HEAP_SIZE      0x1600000  // 22MB
#define GGGX_START     0x2000000
#define GGGX_SIZE      0x1000000  // 16MB

// Arena header for fast allocations
typedef struct {
    uint64_t current_offset;
    uint64_t arena_size;
    uint64_t reset_point;
    uint64_t action_depth;
} ArenaHeader;

// Global memory state (using the existing extern declaration)
extern MemoryState g_memory;

// Provide the actual definition
MemoryState g_memory = {0};

// Initialize memory system
void memory_init(void) {
    if (g_memory.initialized) return;
    
    // Initialize arena
    g_memory.arena = (void*)ARENA_START;
    ArenaHeader* arena = (ArenaHeader*)g_memory.arena;
    arena->current_offset = sizeof(ArenaHeader);
    arena->arena_size = ARENA_SIZE;
    arena->reset_point = sizeof(ArenaHeader);
    arena->action_depth = 0;
    
    // Initialize time travel zones
    uint64_t zone_base = TEMPORAL_START;
    for (int i = 0; i < 3; i++) {
        g_memory.zones[i].entries = (TemporalEntry*)zone_base;
        g_memory.zones[i].used = 0;
        g_memory.zones[i].capacity = ZONE_SIZE / sizeof(TemporalEntry);
        g_memory.zones[i].zone_type = (TimeZone)i;
        zone_base += ZONE_SIZE;
    }
    
    // Initialize heap
    g_memory.heap_current = (uint8_t*)HEAP_START;
    g_memory.total_allocated = 0;
    g_memory.total_freed = 0;
    
    // Initialize GGGX support (integrated with MemoryState)
    g_memory.gggx_manager.traces = (GGGXTrace*)GGGX_START;
    g_memory.gggx_manager.trace_count = 0;
    g_memory.gggx_manager.trace_capacity = 1000; // Max 1000 traces
    g_memory.gggx_manager.metadata = (uint8_t*)(GGGX_START + sizeof(GGGXTrace) * 1000);
    g_memory.gggx_manager.total_traces_created = 0;
    g_memory.gggx_manager.total_traces_cleaned = 0;
    g_memory.gggx_manager.last_cleanup_time = 0;
    
    g_memory.initialized = true;
    
    print_str("Memory system initialized with time travel and GGGX support\n");
    print_str("  Arena: ");
    print_num(ARENA_SIZE / 1024);
    print_str(" KB\n");
    print_str("  Time travel zones: ");
    print_num(3 * ZONE_SIZE / 1024);
    print_str(" KB\n");
    print_str("  Heap: ");
    print_num(HEAP_SIZE / 1024);
    print_str(" KB\n");
    print_str("  GGGX metadata: ");
    print_num(GGGX_SIZE / 1024);
    print_str(" KB\n");
}

// Arena allocation - blazing fast for temporary objects
void* arena_alloc(uint64_t size) {
    if (!g_memory.initialized) memory_init();
    
    ArenaHeader* arena = (ArenaHeader*)g_memory.arena;
    
    // Align to 16 bytes
    size = (size + 15) & ~15;
    
    uint64_t current = arena->current_offset;
    uint64_t new_offset = current + size;
    
    if (new_offset > arena->arena_size) {
        print_str("Arena exhausted! Size requested: ");
        print_num(size);
        print_str("\n");
        return NULL;
    }
    
    arena->current_offset = new_offset;
    return (void*)(ARENA_START + current);
}

// Enter action block - save arena state
void arena_enter_action(void) {
    if (!g_memory.initialized) memory_init();
    
    ArenaHeader* arena = (ArenaHeader*)g_memory.arena;
    arena->action_depth++;
    
    if (arena->action_depth == 1) {
        arena->reset_point = arena->current_offset;
    }
}

// Exit action block - reset arena
void arena_exit_action(void) {
    if (!g_memory.initialized) return;
    
    ArenaHeader* arena = (ArenaHeader*)g_memory.arena;
    if (arena->action_depth > 0) {
        arena->action_depth--;
        
        if (arena->action_depth == 0) {
            arena->current_offset = arena->reset_point;
        }
    }
}

// Reference counted allocation for long-lived objects
void* rc_alloc(uint64_t size) {
    if (!g_memory.initialized) memory_init();
    
    uint64_t total_size = sizeof(RCHeader) + size;
    total_size = (total_size + 15) & ~15;  // Align
    
    if ((uint64_t)(g_memory.heap_current - (uint8_t*)HEAP_START) + total_size > HEAP_SIZE) {
        print_str("Heap exhausted! Size requested: ");
        print_num(size);
        print_str("\n");
        
        // Trigger garbage collection
        temporal_gc();
        
        if ((uint64_t)(g_memory.heap_current - (uint8_t*)HEAP_START) + total_size > HEAP_SIZE) {
            print_str("Still out of memory after GC!\n");
            return NULL;
        }
    }
    
    RCHeader* header = (RCHeader*)g_memory.heap_current;
    header->size = size;
    header->refcount = 1;
    header->flags = 0;
    
    g_memory.heap_current += total_size;
    g_memory.total_allocated += size;
    
    return (void*)(header + 1);
}

// Increase reference count
void rc_inc(void* ptr) {
    if (!ptr) return;
    
    RCHeader* header = ((RCHeader*)ptr) - 1;
    if (header->refcount < 0xFFFF) {
        header->refcount++;
    }
}

// Decrease reference count with automatic cleanup
void rc_dec(void* ptr) {
    if (!ptr) return;
    
    RCHeader* header = ((RCHeader*)ptr) - 1;
    if (header->refcount > 0) {
        header->refcount--;
        
        if (header->refcount == 0) {
            g_memory.total_freed += header->size;
            header->flags |= RC_FLAG_MARKED;
        }
    }
}

// Get current reference count
uint16_t rc_count(void* ptr) {
    if (!ptr) return 0;
    
    RCHeader* header = ((RCHeader*)ptr) - 1;
    return header->refcount;
}

// Allocate in time travel zone
void* temporal_alloc(TimeZone zone, uint64_t size) {
    if (!g_memory.initialized) memory_init();
    
    if (zone > ZONE_FUTURE) return NULL;
    
    ZoneManager* zm = &g_memory.zones[zone];
    
    if (zm->used >= zm->capacity) {
        print_str("Time travel zone full: ");
        print_num(zone);
        print_str("\n");
        return NULL;
    }
    
    // Allocate actual data from ref-counted heap
    void* data = rc_alloc(size);
    if (!data) return NULL;
    
    // Mark as temporal
    RCHeader* header = ((RCHeader*)data) - 1;
    header->flags |= RC_FLAG_TEMPORAL;
    
    // Create temporal entry
    TemporalEntry* entry = &zm->entries[zm->used++];
    entry->value_ptr = data;
    entry->timeline_id = 1; // Use simple timeline for now
    entry->temporal_offset = 0;
    entry->creating_timeline = 1;
    entry->next = NULL;
    entry->prev = (zm->used > 1) ? &zm->entries[zm->used - 2] : NULL;
    
    // Register as GC root if in PRESENT zone
    if (zone == ZONE_PRESENT) {
        // Note: gc_add_root would be called here if available
    }
    
    return data;
}

// Move value between time travel zones
void* temporal_move(void* ptr, TimeZone from_zone, TimeZone to_zone) {
    if (!ptr || from_zone > ZONE_FUTURE || to_zone > ZONE_FUTURE) {
        return NULL;
    }
    
    RCHeader* header = ((RCHeader*)ptr) - 1;
    void* new_ptr = temporal_alloc(to_zone, header->size);
    
    if (new_ptr) {
        // Copy data
        uint8_t* src = (uint8_t*)ptr;
        uint8_t* dst = (uint8_t*)new_ptr;
        for (uint64_t i = 0; i < header->size; i++) {
            dst[i] = src[i];
        }
        
        // Decrease ref in old zone
        rc_dec(ptr);
    }
    
    return new_ptr;
}

// Memory statistics with time travel and GGGX info
void memory_stats(void) {
    print_str("\n=== MEMORY STATISTICS ===\n");
    
    // Arena stats
    ArenaHeader* arena = (ArenaHeader*)g_memory.arena;
    uint64_t arena_used = arena->current_offset - sizeof(ArenaHeader);
    print_str("Arena: ");
    print_num(arena_used / 1024);
    print_str(" KB used of ");
    print_num(ARENA_SIZE / 1024);
    print_str(" KB (");
    print_num(arena_used * 100 / ARENA_SIZE);
    print_str("%)\n");
    
    // Heap stats
    uint64_t heap_used = (uint64_t)(g_memory.heap_current - (uint8_t*)HEAP_START);
    print_str("Heap: ");
    print_num(heap_used / 1024);
    print_str(" KB used of ");
    print_num(HEAP_SIZE / 1024);
    print_str(" KB (");
    print_num(heap_used * 100 / HEAP_SIZE);
    print_str("%)\n");
    
    print_str("Total allocated: ");
    print_num(g_memory.total_allocated / 1024);
    print_str(" KB\n");
    print_str("Total freed: ");
    print_num(g_memory.total_freed / 1024);
    print_str(" KB\n");
    print_str("Live objects: ");
    print_num((g_memory.total_allocated - g_memory.total_freed) / 1024);
    print_str(" KB\n");
    
    // Time travel zone stats
    const char* zone_names[] = {"Past", "Present", "Future"};
    for (int i = 0; i < 3; i++) {
        print_str(zone_names[i]);
        print_str(" zone: ");
        print_num(g_memory.zones[i].used);
        print_str(" entries\n");
    }
    
    // GGGX stats
    print_str("GGGX traces: ");
    print_num(g_memory.gggx_manager.trace_count);
    print_str(" active, ");
    print_num(g_memory.gggx_manager.total_traces_created);
    print_str(" created, ");
    print_num(g_memory.gggx_manager.total_traces_cleaned);
    print_str(" cleaned\n");
    
    print_str("======================\n");
}

// Garbage collection for time travel zones
void temporal_gc(void) {
    // Call the temporal GC function from temporal_gc.c
    temporal_gc_collect();
}

// Test the memory system
void memory_test(void) {
    print_str("\n=== MEMORY SYSTEM TEST ===\n");
    
    // Test arena allocation
    print_str("Testing arena allocation...\n");
    void* a1 = arena_alloc(100);
    void* a2 = arena_alloc(200);
    print_str("Arena allocs: ");
    print_num((uint64_t)a1);
    print_str(", ");
    print_num((uint64_t)a2);
    print_str("\n");
    
    // Test action blocks
    print_str("Testing action blocks...\n");
    arena_enter_action();
    void* a3 = arena_alloc(300);
    print_str("Inside action: ");
    print_num((uint64_t)a3);
    print_str("\n");
    arena_exit_action();
    print_str("After action exit\n");
    
    // Test reference counting
    print_str("Testing reference counting...\n");
    void* r1 = rc_alloc(64);
    print_str("RC alloc: ");
    print_num((uint64_t)r1);
    print_str(", count: ");
    print_num(rc_count(r1));
    print_str("\n");
    
    rc_inc(r1);
    print_str("After inc: ");
    print_num(rc_count(r1));
    print_str("\n");
    
    rc_dec(r1);
    rc_dec(r1);
    print_str("After 2x dec: ");
    print_num(rc_count(r1));
    print_str("\n");
    
    // Test time travel zones
    print_str("Testing time travel zones...\n");
    void* t1 = temporal_alloc(ZONE_PRESENT, 128);
    print_str("Present alloc: ");
    print_num((uint64_t)t1);
    print_str("\n");
    
    void* t2 = temporal_move(t1, ZONE_PRESENT, ZONE_FUTURE);
    print_str("Moved to future: ");
    print_num((uint64_t)t2);
    print_str("\n");
    
    memory_stats();
}

// GGGX-specific allocation for computational traces
void* gggx_alloc_trace(uint64_t size) {
    if (!g_memory.initialized) memory_init();
    
    if (g_memory.gggx_manager.trace_count >= g_memory.gggx_manager.trace_capacity) {
        print_str("GGGX trace capacity exceeded!\n");
        return NULL;
    }
    
    // Allocate trace data from heap
    void* trace_data = rc_alloc(size);
    if (!trace_data) return NULL;
    
    // Create GGGX trace entry
    GGGXTrace* trace = &g_memory.gggx_manager.traces[g_memory.gggx_manager.trace_count++];
    trace->trace_id = g_memory.gggx_manager.trace_count;
    trace->trace_data = trace_data;
    trace->trace_size = size;
    trace->creation_timeline = 1; // Use simple timeline for now
    trace->is_active = true;
    trace->access_count = 0;
    trace->last_access_time = 0;
    trace->complexity_score = 0;
    trace->confidence_level = 50; // Default confidence
    
    // Mark as GGGX trace in RC header
    RCHeader* header = ((RCHeader*)trace_data) - 1;
    header->flags |= RC_FLAG_TEMPORAL; // Use temporal flag for GGGX traces
    
    g_memory.gggx_manager.total_traces_created++;
    
    return trace_data;
}

// Activate a GGGX trace
void gggx_trace_activate(uint64_t trace_id) {
    if (trace_id > 0 && trace_id <= g_memory.gggx_manager.trace_count) {
        GGGXTrace* trace = &g_memory.gggx_manager.traces[trace_id - 1];
        trace->is_active = true;
        trace->last_access_time = 1; // Simple timeline counter
    }
}

// Deactivate a GGGX trace
void gggx_trace_deactivate(uint64_t trace_id) {
    if (trace_id > 0 && trace_id <= g_memory.gggx_manager.trace_count) {
        GGGXTrace* trace = &g_memory.gggx_manager.traces[trace_id - 1];
        trace->is_active = false;
    }
}

// Record access to a GGGX trace
void gggx_trace_access(uint64_t trace_id) {
    if (trace_id > 0 && trace_id <= g_memory.gggx_manager.trace_count) {
        GGGXTrace* trace = &g_memory.gggx_manager.traces[trace_id - 1];
        trace->access_count++;
        trace->last_access_time = 1; // Simple timeline counter
    }
}

// Get trace ID from trace data pointer
uint64_t gggx_get_trace_id(void* trace_data) {
    for (uint32_t i = 0; i < g_memory.gggx_manager.trace_count; i++) {
        if (g_memory.gggx_manager.traces[i].trace_data == trace_data) {
            return g_memory.gggx_manager.traces[i].trace_id;
        }
    }
    return 0; // Not found
}

// Set complexity score for a trace
void gggx_set_trace_complexity(uint64_t trace_id, uint32_t complexity) {
    if (trace_id > 0 && trace_id <= g_memory.gggx_manager.trace_count) {
        GGGXTrace* trace = &g_memory.gggx_manager.traces[trace_id - 1];
        trace->complexity_score = complexity;
    }
}

// Set confidence level for a trace
void gggx_set_trace_confidence(uint64_t trace_id, uint16_t confidence) {
    if (trace_id > 0 && trace_id <= g_memory.gggx_manager.trace_count) {
        GGGXTrace* trace = &g_memory.gggx_manager.traces[trace_id - 1];
        trace->confidence_level = confidence;
    }
}

// Clean up old GGGX traces
void gggx_trace_cleanup_old(void) {
    uint32_t cleaned = 0;
    
    for (uint32_t i = 0; i < g_memory.gggx_manager.trace_count; i++) {
        GGGXTrace* trace = &g_memory.gggx_manager.traces[i];
        
        // Clean up traces that are old and inactive
        if (trace->is_active == false && trace->access_count > 10) {
            // Decrease ref count on trace data
            rc_dec(trace->trace_data);
            trace->trace_data = NULL;
            cleaned++;
        }
    }
    
    g_memory.gggx_manager.total_traces_cleaned += cleaned;
    g_memory.gggx_manager.last_cleanup_time = 1; // Simple timeline counter
    
    if (cleaned > 0) {
        print_str("GGGX cleanup: cleaned ");
        print_num(cleaned);
        print_str(" old traces\n");
    }
}

// Print GGGX trace statistics
void gggx_trace_stats(void) {
    print_str("\n=== GGGX TRACE STATISTICS ===\n");
    print_str("Total traces: ");
    print_num(g_memory.gggx_manager.trace_count);
    print_str("\n");
    print_str("Active traces: ");
    
    uint32_t active_count = 0;
    uint32_t total_access = 0;
    uint32_t total_complexity = 0;
    
    for (uint32_t i = 0; i < g_memory.gggx_manager.trace_count; i++) {
        GGGXTrace* trace = &g_memory.gggx_manager.traces[i];
        if (trace->is_active) {
            active_count++;
        }
        total_access += trace->access_count;
        total_complexity += trace->complexity_score;
    }
    
    print_num(active_count);
    print_str("\n");
    print_str("Total traces created: ");
    print_num(g_memory.gggx_manager.total_traces_created);
    print_str("\n");
    print_str("Total traces cleaned: ");
    print_num(g_memory.gggx_manager.total_traces_cleaned);
    print_str("\n");
    print_str("Total access count: ");
    print_num(total_access);
    print_str("\n");
    print_str("Average complexity: ");
    if (g_memory.gggx_manager.trace_count > 0) {
        print_num(total_complexity / g_memory.gggx_manager.trace_count);
    } else {
        print_str("0");
    }
    print_str("\n");
    print_str("=============================\n");
}