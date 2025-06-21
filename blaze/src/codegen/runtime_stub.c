// RUNTIME STUB FOR GENERATED EXECUTABLES
// This code is compiled into the generated executables to provide memory management

// Minimal runtime without stdlib dependencies
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

#define NULL ((void*)0)
#define true 1
#define false 0

// Memory layout constants (must match memory_manager.c)
#define ARENA_START    0x100000
#define ARENA_SIZE     0x600000
#define TEMPORAL_START 0x700000
#define ZONE_SIZE      0x100000
#define HEAP_START     0xA00000
#define HEAP_SIZE      0x1600000

// Minimal structures needed
typedef struct {
    uint64_t current_offset;
    uint64_t arena_size;
    uint64_t reset_point;
    uint64_t action_depth;
} ArenaHeader;

typedef struct {
    uint32_t size;
    uint16_t refcount;
    uint16_t flags;
} RCHeader;

// Global state (simplified)
static struct {
    ArenaHeader* arena;
    uint8_t* heap_current;
    int initialized;
} g_memory = {0};

// System call wrapper
static long syscall6(long num, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    __asm__ volatile (
        "mov %1, %%rax\n"
        "mov %2, %%rdi\n"
        "mov %3, %%rsi\n"
        "mov %4, %%rdx\n"
        "mov %5, %%r10\n"
        "mov %6, %%r8\n"
        "mov %7, %%r9\n"
        "syscall\n"
        "mov %%rax, %0\n"
        : "=r"(ret)
        : "r"(num), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6)
        : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9", "memory"
    );
    return ret;
}

// Initialize memory system
void memory_init(void) {
    if (g_memory.initialized) return;
    
    // Map memory regions using mmap
    // mmap(addr, length, prot, flags, fd, offset)
    // prot = PROT_READ|PROT_WRITE = 3
    // flags = MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED = 0x32
    
    // Map arena
    syscall6(9, ARENA_START, ARENA_SIZE, 3, 0x32, -1, 0);
    
    // Map temporal zones
    syscall6(9, TEMPORAL_START, 3 * ZONE_SIZE, 3, 0x32, -1, 0);
    
    // Map heap
    syscall6(9, HEAP_START, HEAP_SIZE, 3, 0x32, -1, 0);
    
    // Initialize arena
    g_memory.arena = (ArenaHeader*)ARENA_START;
    g_memory.arena->current_offset = sizeof(ArenaHeader);
    g_memory.arena->arena_size = ARENA_SIZE;
    g_memory.arena->reset_point = sizeof(ArenaHeader);
    g_memory.arena->action_depth = 0;
    
    // Initialize heap
    g_memory.heap_current = (uint8_t*)HEAP_START;
    
    g_memory.initialized = 1;
}

// Arena allocation
void* arena_alloc(uint64_t size) {
    if (!g_memory.initialized) memory_init();
    
    // Align to 16 bytes
    size = (size + 15) & ~15;
    
    uint64_t current = g_memory.arena->current_offset;
    uint64_t new_offset = current + size;
    
    if (new_offset > g_memory.arena->arena_size) {
        return NULL;
    }
    
    g_memory.arena->current_offset = new_offset;
    return (void*)(ARENA_START + current);
}

// Reference counted allocation
void* rc_alloc(uint64_t size) {
    if (!g_memory.initialized) memory_init();
    
    uint64_t total_size = sizeof(RCHeader) + size;
    total_size = (total_size + 15) & ~15;
    
    if ((uint64_t)(g_memory.heap_current - (uint8_t*)HEAP_START) + total_size > HEAP_SIZE) {
        return NULL;
    }
    
    RCHeader* header = (RCHeader*)g_memory.heap_current;
    header->size = size;
    header->refcount = 1;
    header->flags = 0;
    
    g_memory.heap_current += total_size;
    
    return (void*)(header + 1);
}

// Arena action blocks
void arena_enter_action(void) {
    if (!g_memory.initialized) memory_init();
    
    g_memory.arena->action_depth++;
    
    if (g_memory.arena->action_depth == 1) {
        g_memory.arena->reset_point = g_memory.arena->current_offset;
    }
}

void arena_exit_action(void) {
    if (!g_memory.initialized) return;
    
    if (g_memory.arena->action_depth > 0) {
        g_memory.arena->action_depth--;
        
        if (g_memory.arena->action_depth == 0) {
            g_memory.arena->current_offset = g_memory.arena->reset_point;
        }
    }
}

// Reference counting
void rc_inc(void* ptr) {
    if (!ptr) return;
    
    RCHeader* header = ((RCHeader*)ptr) - 1;
    if (header->refcount < 0xFFFF) {
        header->refcount++;
    }
}

void rc_dec(void* ptr) {
    if (!ptr) return;
    
    RCHeader* header = ((RCHeader*)ptr) - 1;
    if (header->refcount > 0) {
        header->refcount--;
    }
}

// Temporal allocation (simplified)
void* temporal_alloc(int zone, uint64_t size) {
    // For now, just use rc_alloc
    return rc_alloc(size);
}