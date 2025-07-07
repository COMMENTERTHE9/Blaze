// BLAZE COMPETENT GARBAGE COLLECTOR
// Mark & sweep with time travel and GGGX support

#include "blaze_internals.h"

// Memory layout constants (from memory_manager.c)
#define HEAP_START     0xA00000
#define HEAP_SIZE      0x1600000  // 22MB
#define GGGX_START     0x2000000
#define GGGX_SIZE      0x1000000  // 16MB

// Forward declarations
typedef struct GCRoot GCRoot;
typedef struct GCStats GCStats;
typedef struct TimelineLink TimelineLink;

// Function declarations
void temporal_gc_collect(void);
void gc_add_root(void* ptr, const char* name);
void gc_remove_root(void* ptr);
void gc_add_timeline_link(void* from, void* to, TimeZone from_zone, TimeZone to_zone);
uint64_t gc_get_timeline(void);
uint64_t gc_new_timeline(void);

// Root set entry
struct GCRoot {
    void* ptr;
    const char* name;
    GCRoot* next;
};

// Timeline link between objects across zones
struct TimelineLink {
    void* from_obj;
    void* to_obj;
    TimeZone from_zone;
    TimeZone to_zone;
    uint64_t timeline_id;
    TimelineLink* next;
};

// GC statistics
struct GCStats {
    uint64_t marked_objects;
    uint64_t freed_objects;
    uint64_t freed_bytes;
    uint64_t moved_objects;
    uint64_t gggx_traces_cleaned;
    uint64_t cycle_count;
    uint64_t last_gc_time;
};

// Global GC state
static struct {
    GCRoot* roots;
    TimelineLink* timeline_links;
    GCStats stats;
    bool gc_in_progress;
    uint64_t current_timeline;
    uint64_t mark_color;  // Increments each GC cycle
} g_gc = {0};

// Get the RCHeader from an object pointer
static inline RCHeader* get_header(void* ptr) {
    if (!ptr) return NULL;
    return ((RCHeader*)ptr) - 1;
}

// Check if object is marked in current GC cycle
static bool is_marked(void* ptr) {
    RCHeader* header = get_header(ptr);
    if (!header) return false;
    
    // We use the upper 16 bits of flags for mark color
    uint16_t obj_color = (header->flags >> 16) & 0xFFFF;
    return obj_color == (g_gc.mark_color & 0xFFFF);
}

// Mark object as reachable in current GC cycle
static void mark_object(void* ptr) {
    RCHeader* header = get_header(ptr);
    if (!header) return;
    
    // Set mark color in upper 16 bits
    header->flags = (header->flags & 0xFFFF) | ((g_gc.mark_color & 0xFFFF) << 16);
    g_gc.stats.marked_objects++;
}

// Register a GC root
void gc_add_root(void* ptr, const char* name) {
    if (!ptr) return;
    
    GCRoot* root = (GCRoot*)arena_alloc(sizeof(GCRoot));
    root->ptr = ptr;
    root->name = name;
    root->next = g_gc.roots;
    g_gc.roots = root;
}

// Remove a GC root
void gc_remove_root(void* ptr) {
    GCRoot** current = &g_gc.roots;
    while (*current) {
        if ((*current)->ptr == ptr) {
            GCRoot* to_remove = *current;
            *current = to_remove->next;
            // Note: We don't free since it's in arena
            return;
        }
        current = &(*current)->next;
    }
}

// Add a timeline link between objects
void gc_add_timeline_link(void* from, void* to, TimeZone from_zone, TimeZone to_zone) {
    if (!from || !to) return;
    
    TimelineLink* link = (TimelineLink*)arena_alloc(sizeof(TimelineLink));
    link->from_obj = from;
    link->to_obj = to;
    link->from_zone = from_zone;
    link->to_zone = to_zone;
    link->timeline_id = g_gc.current_timeline;
    link->next = g_gc.timeline_links;
    g_gc.timeline_links = link;
}

// Mark phase - traverse object graph from roots
static void gc_mark_phase(void) {
    // Start from root set
    for (GCRoot* root = g_gc.roots; root; root = root->next) {
        if (!is_marked(root->ptr)) {
            mark_object(root->ptr);
        }
    }
    
    // Mark from PRESENT zone stack frames
    uint64_t* stack_base = (uint64_t*)0x7FFFFFFFFFFF;  // Typical stack top
    uint64_t* stack_ptr = (uint64_t*)__builtin_frame_address(0);
    
    // Scan stack conservatively for pointers
    for (uint64_t* p = stack_ptr; p < stack_base; p++) {
        uint64_t val = *p;
        // Check if it looks like a heap pointer
        if (val >= HEAP_START && val < (HEAP_START + HEAP_SIZE)) {
            void* potential_ptr = (void*)val;
            RCHeader* header = get_header(potential_ptr);
            if (header && header->size > 0 && header->size < HEAP_SIZE) {
                if (!is_marked(potential_ptr)) {
                    mark_object(potential_ptr);
                }
            }
        }
    }
    
    // Follow timeline links
    bool changed = true;
    while (changed) {
        changed = false;
        for (TimelineLink* link = g_gc.timeline_links; link; link = link->next) {
            if (is_marked(link->from_obj) && !is_marked(link->to_obj)) {
                mark_object(link->to_obj);
                changed = true;
            }
            // Bidirectional for temporal consistency
            if (is_marked(link->to_obj) && !is_marked(link->from_obj)) {
                mark_object(link->from_obj);
                changed = true;
            }
        }
    }
}

// Sweep phase - free unmarked objects
static void gc_sweep_phase(void) {
    uint8_t* heap_ptr = (uint8_t*)HEAP_START;
    uint8_t* heap_end = (uint8_t*)(HEAP_START + HEAP_SIZE);
    
    while (heap_ptr < heap_end && heap_ptr < g_memory.heap_current) {
        RCHeader* header = (RCHeader*)heap_ptr;
        
        // Skip if not a valid header
        if (header->size == 0 || header->size > HEAP_SIZE) {
            heap_ptr += 16;  // Try next alignment
            continue;
        }
        
        uint64_t total_size = sizeof(RCHeader) + header->size;
        total_size = (total_size + 15) & ~15;  // Align
        
        if (!is_marked((void*)(header + 1))) {
            // Object is garbage
            if (!(header->flags & RC_FLAG_MARKED)) {
                // Not already freed
                g_gc.stats.freed_objects++;
                g_gc.stats.freed_bytes += header->size;
                header->flags |= RC_FLAG_MARKED;
            }
        }
        
        heap_ptr += total_size;
    }
}

// Zone migration - move objects between time travel zones
static void gc_migrate_zones(void) {
    // Migrate old PRESENT objects to PAST
    ZoneManager* present = &g_memory.zones[ZONE_PRESENT];
    ZoneManager* past = &g_memory.zones[ZONE_PAST];
    
    for (uint64_t i = 0; i < present->used; i++) {
        TemporalEntry* entry = &present->entries[i];
        
        // Check if object is old enough to migrate
        if (g_gc.current_timeline - entry->timeline_id > 100) {  // Arbitrary threshold
            // Move to past zone if there's space
            if (past->used < past->capacity) {
                past->entries[past->used] = *entry;
                past->used++;
                
                // Remove from present (swap with last)
                present->entries[i] = present->entries[--present->used];
                i--;  // Recheck this slot
                
                g_gc.stats.moved_objects++;
            }
        }
    }
}

// Clean up old GGGX traces
static void gc_cleanup_gggx_traces(void) {
    uint32_t cleaned = 0;
    
    for (uint32_t i = 0; i < g_memory.gggx_manager.trace_count; i++) {
        GGGXTrace* trace = &g_memory.gggx_manager.traces[i];
        
        // Clean up traces that are old and inactive
        if (trace->is_active == false && trace->access_count > 5) {
            // Decrease ref count on trace data
            rc_dec(trace->trace_data);
            trace->trace_data = NULL;
            cleaned++;
        }
    }
    
    g_memory.gggx_manager.total_traces_cleaned += cleaned;
    g_memory.gggx_manager.last_cleanup_time = g_gc.current_timeline;
    
    g_gc.stats.gggx_traces_cleaned = cleaned;
    
    if (cleaned > 0) {
        print_str("GC: cleaned ");
        print_num(cleaned);
        print_str(" old GGGX traces\n");
    }
}

// Main garbage collection function
void temporal_gc_collect(void) {
    if (g_gc.gc_in_progress) return;
    
    g_gc.gc_in_progress = true;
    g_gc.mark_color++;
    g_gc.stats.cycle_count++;
    
    print_str("Starting garbage collection cycle ");
    print_num(g_gc.stats.cycle_count);
    print_str("\n");
    
    // Reset stats for this cycle
    g_gc.stats.marked_objects = 0;
    g_gc.stats.freed_objects = 0;
    g_gc.stats.freed_bytes = 0;
    g_gc.stats.moved_objects = 0;
    g_gc.stats.gggx_traces_cleaned = 0;
    
    // Mark phase
    gc_mark_phase();
    
    // Sweep phase
    gc_sweep_phase();
    
    // Zone migration
    gc_migrate_zones();
    
    // GGGX trace cleanup
    gc_cleanup_gggx_traces();
    
    print_str("GC complete: marked ");
    print_num(g_gc.stats.marked_objects);
    print_str(" objects, freed ");
    print_num(g_gc.stats.freed_objects);
    print_str(" objects (");
    print_num(g_gc.stats.freed_bytes / 1024);
    print_str(" KB), moved ");
    print_num(g_gc.stats.moved_objects);
    print_str(" objects, cleaned ");
    print_num(g_gc.stats.gggx_traces_cleaned);
    print_str(" GGGX traces\n");
    
    g_gc.gc_in_progress = false;
}

// Get current timeline ID
uint64_t gc_get_timeline(void) {
    return g_gc.current_timeline;
}

// Create new timeline (for divergence)
uint64_t gc_new_timeline(void) {
    return ++g_gc.current_timeline;
}

// Debug functions
void gc_print_roots(void) {
    print_str("=== GC ROOTS ===\n");
    for (GCRoot* root = g_gc.roots; root; root = root->next) {
        print_str("Root: ");
        print_str(root->name);
        print_str(" -> ");
        print_num((uint64_t)root->ptr);
        print_str("\n");
    }
    print_str("===============\n");
}

void gc_print_timeline_links(void) {
    print_str("=== TIMELINE LINKS ===\n");
    const char* zone_names[] = {"Past", "Present", "Future"};
    
    for (TimelineLink* link = g_gc.timeline_links; link; link = link->next) {
        print_str("  ");
        print_num((uint64_t)link->from_obj);
        print_str(" (");
        print_str(zone_names[link->from_zone]);
        print_str(") -> ");
        print_num((uint64_t)link->to_obj);
        print_str(" (");
        print_str(zone_names[link->to_zone]);
        print_str(") [timeline ");
        print_num(link->timeline_id);
        print_str("]\n");
    }
    print_str("===================\n");
}

void gc_print_stats(void) {
    print_str("=== GC STATISTICS ===\n");
    print_str("Cycles: ");
    print_num(g_gc.stats.cycle_count);
    print_str("\n");
    print_str("Total marked: ");
    print_num(g_gc.stats.marked_objects);
    print_str("\n");
    print_str("Total freed: ");
    print_num(g_gc.stats.freed_objects);
    print_str("\n");
    print_str("Total freed bytes: ");
    print_num(g_gc.stats.freed_bytes / 1024);
    print_str(" KB\n");
    print_str("Total moved: ");
    print_num(g_gc.stats.moved_objects);
    print_str("\n");
    print_str("GGGX traces cleaned: ");
    print_num(g_gc.stats.gggx_traces_cleaned);
    print_str("\n");
    print_str("===================\n");
}