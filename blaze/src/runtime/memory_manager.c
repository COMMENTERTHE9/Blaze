// BLAZE MEMORY MANAGER - Three-Tier System
// Arena + Reference Counting + Temporal Zones

#include "blaze_internals.h"

// Memory Layout:
// 0x100000-0x700000: Arena pool (6MB)
// 0x700000-0xA00000: Temporal zones (3MB total)
//   0x700000-0x800000: Past zone (1MB)
//   0x800000-0x900000: Present zone (1MB)
//   0x900000-0xA00000: Future zone (1MB)
// 0xA00000-0x2000000: Ref-counted heap (22MB)
// 0x2000000-0x3000000: GC metadata (16MB)

#define ARENA_START    0x100000
#define ARENA_SIZE     0x600000   // 6MB
#define TEMPORAL_START 0x700000
#define ZONE_SIZE      0x100000   // 1MB per zone
#define HEAP_START     0xA00000
#define HEAP_SIZE      0x1600000  // 22MB
#define META_START     0x2000000
#define META_SIZE      0x1000000  // 16MB

// Arena header
typedef struct {
    uint64_t current_offset;
    uint64_t arena_size;
    uint64_t reset_point;
    uint64_t action_depth;  // Nested action blocks
} ArenaHeader;

// Reference count header (precedes each allocation)
typedef struct {
    uint32_t size;
    uint16_t refcount;
    uint16_t flags;
    #define RC_FLAG_TEMPORAL  0x0001
    #define RC_FLAG_WEAK      0x0002
    #define RC_FLAG_ARRAY4D   0x0004
    #define RC_FLAG_MARKED    0x0008
} RCHeader;

// Temporal zone entry
typedef struct TemporalEntry {
    void* value_ptr;
    uint64_t timeline_id;
    int32_t temporal_offset;
    uint32_t creating_timeline;
    struct TemporalEntry* next;
    struct TemporalEntry* prev;
} TemporalEntry;

// Zone manager
typedef struct {
    TemporalEntry* entries;
    uint64_t used;
    uint64_t capacity;
    TimeZone zone_type;
} ZoneManager;

// Global memory state
static struct {
    ArenaHeader* arena;
    ZoneManager zones[3];  // Past, Present, Future
    uint8_t* heap_current;
    uint64_t total_allocated;
    uint64_t total_freed;
    bool initialized;
} g_memory = {0};

// Initialize memory system
void memory_init(void) {
    if (g_memory.initialized) return;
    
    // Initialize arena
    g_memory.arena = (ArenaHeader*)ARENA_START;
    g_memory.arena->current_offset = sizeof(ArenaHeader);
    g_memory.arena->arena_size = ARENA_SIZE;
    g_memory.arena->reset_point = sizeof(ArenaHeader);
    g_memory.arena->action_depth = 0;
    
    // Initialize temporal zones
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
    
    // Clear metadata area
    uint8_t* meta = (uint8_t*)META_START;
    for (uint64_t i = 0; i < META_SIZE; i++) {
        meta[i] = 0;
    }
    
    g_memory.initialized = true;
    
    print_str("Memory system initialized\n");
    print_str("  Arena: ");
    print_num(ARENA_SIZE / 1024);
    print_str(" KB\n");
    print_str("  Temporal zones: ");
    print_num(3 * ZONE_SIZE / 1024);
    print_str(" KB\n");
    print_str("  Heap: ");
    print_num(HEAP_SIZE / 1024);
    print_str(" KB\n");
}

// Arena allocation - blazing fast!
void* arena_alloc(uint64_t size) {
    if (!g_memory.initialized) memory_init();
    
    // Align to 16 bytes
    size = (size + 15) & ~15;
    
    uint64_t current = g_memory.arena->current_offset;
    uint64_t new_offset = current + size;
    
    if (new_offset > g_memory.arena->arena_size) {
        print_str("Arena exhausted! Size requested: ");
        print_num(size);
        print_str("\n");
        return NULL;
    }
    
    g_memory.arena->current_offset = new_offset;
    return (void*)(ARENA_START + current);
}

// Enter action block - save arena state
void arena_enter_action(void) {
    if (!g_memory.initialized) memory_init();
    
    g_memory.arena->action_depth++;
    
    // Save reset point for nested actions
    if (g_memory.arena->action_depth == 1) {
        g_memory.arena->reset_point = g_memory.arena->current_offset;
    }
}

// Exit action block - reset arena
void arena_exit_action(void) {
    if (!g_memory.initialized) return;
    
    if (g_memory.arena->action_depth > 0) {
        g_memory.arena->action_depth--;
        
        // Reset arena when exiting outermost action
        if (g_memory.arena->action_depth == 0) {
            g_memory.arena->current_offset = g_memory.arena->reset_point;
        }
    }
}

// Reference counted allocation
void* rc_alloc(uint64_t size) {
    if (!g_memory.initialized) memory_init();
    
    // Need space for header + data
    uint64_t total_size = sizeof(RCHeader) + size;
    total_size = (total_size + 15) & ~15;  // Align
    
    // Check heap space
    if ((uint64_t)(g_memory.heap_current - (uint8_t*)HEAP_START) + total_size > HEAP_SIZE) {
        print_str("Heap exhausted! Size requested: ");
        print_num(size);
        print_str("\n");
        // TODO: Trigger compaction or GC
        return NULL;
    }
    
    RCHeader* header = (RCHeader*)g_memory.heap_current;
    header->size = size;
    header->refcount = 1;
    header->flags = 0;
    
    g_memory.heap_current += total_size;
    g_memory.total_allocated += size;
    
    // Return pointer to data (after header)
    return (void*)(header + 1);
}

// Increase reference count
void rc_inc(void* ptr) {
    if (!ptr) return;
    
    RCHeader* header = ((RCHeader*)ptr) - 1;
    if (header->refcount < 0xFFFF) {  // Prevent overflow
        header->refcount++;
    }
}

// Decrease reference count
void rc_dec(void* ptr) {
    if (!ptr) return;
    
    RCHeader* header = ((RCHeader*)ptr) - 1;
    if (header->refcount > 0) {
        header->refcount--;
        
        if (header->refcount == 0) {
            // Free the memory
            g_memory.total_freed += header->size;
            
            // For now, just mark as free
            header->flags |= RC_FLAG_MARKED;
            
            // TODO: Add to free list for reuse
        }
    }
}

// Get current reference count
uint16_t rc_count(void* ptr) {
    if (!ptr) return 0;
    
    RCHeader* header = ((RCHeader*)ptr) - 1;
    return header->refcount;
}

// Allocate in temporal zone
void* temporal_alloc(TimeZone zone, uint64_t size) {
    if (!g_memory.initialized) memory_init();
    
    if (zone > ZONE_FUTURE) return NULL;
    
    ZoneManager* zm = &g_memory.zones[zone];
    
    // For now, use zone entries for metadata
    if (zm->used >= zm->capacity) {
        print_str("Temporal zone full: ");
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
    entry->timeline_id = 0;  // TODO: Get current timeline
    entry->temporal_offset = 0;  // TODO: Calculate offset
    entry->creating_timeline = 0;  // TODO: Track timeline
    entry->next = NULL;
    entry->prev = (zm->used > 1) ? &zm->entries[zm->used - 2] : NULL;
    
    return data;
}

// Move value between temporal zones
void* temporal_move(void* ptr, TimeZone from_zone, TimeZone to_zone) {
    if (!ptr || from_zone > ZONE_FUTURE || to_zone > ZONE_FUTURE) {
        return NULL;
    }
    
    // For now, just allocate in new zone and copy
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

// Memory statistics
void memory_stats(void) {
    print_str("\n=== MEMORY STATISTICS ===\n");
    
    // Arena stats
    uint64_t arena_used = g_memory.arena->current_offset - sizeof(ArenaHeader);
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
    
    // Temporal zone stats
    for (int i = 0; i < 3; i++) {
        const char* zone_names[] = {"Past", "Present", "Future"};
        print_str(zone_names[i]);
        print_str(" zone: ");
        print_num(g_memory.zones[i].used);
        print_str(" entries\n");
    }
    
    print_str("======================\n");
}

// Garbage collection for temporal zones
void temporal_gc(void) {
    print_str("Running temporal GC...\n");
    
    // TODO: Implement mark & sweep for temporal zones
    // 1. Mark all reachable objects from roots
    // 2. Sweep unmarked objects
    // 3. Compact zones if needed
    
    print_str("Temporal GC complete\n");
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
    
    // Test temporal zones
    print_str("Testing temporal zones...\n");
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