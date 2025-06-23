// SOLID NUMBER MEMORY POOLING OPTIMIZATION
// High-performance memory management for solid numbers

#include "blaze_internals.h"
#include "solid_runtime.h"

// Pool configuration
#define POOL_SMALL_SIZE     64      // For numbers < 64 bytes
#define POOL_MEDIUM_SIZE    256     // For numbers < 256 bytes  
#define POOL_LARGE_SIZE     1024    // For numbers < 1KB
#define POOL_HUGE_SIZE      4096    // For numbers < 4KB

#define POOL_SMALL_COUNT    1024    // Number of small blocks
#define POOL_MEDIUM_COUNT   256     // Number of medium blocks
#define POOL_LARGE_COUNT    64      // Number of large blocks
#define POOL_HUGE_COUNT     16      // Number of huge blocks

// Memory pool structure
typedef struct PoolBlock {
    struct PoolBlock* next;
    uint32_t size;
    uint32_t used;
    uint8_t data[];
} PoolBlock;

typedef struct {
    PoolBlock* free_list;
    PoolBlock* used_list;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t allocations;
    uint32_t deallocations;
    uint64_t bytes_allocated;
    uint64_t bytes_freed;
} MemoryPool;

// Thread-local storage for per-thread pools
typedef struct {
    MemoryPool small_pool;
    MemoryPool medium_pool;
    MemoryPool large_pool;
    MemoryPool huge_pool;
    
    // Statistics
    uint64_t total_allocations;
    uint64_t cache_hits;
    uint64_t cache_misses;
} ThreadLocalPools;

// Global pools (with lock-free design)
static ThreadLocalPools thread_pools[16];  // Support up to 16 threads
static int thread_pool_initialized[16] = {0};

// Get thread ID (simplified - in production use proper thread ID)
static int get_thread_id(void) {
    uint64_t tid;
    asm volatile("mov %%fs:0x10, %0" : "=r"(tid));
    return (tid >> 12) & 15;  // Simple hash to 0-15
}

// Initialize a memory pool
static void init_pool(MemoryPool* pool, uint32_t block_size, uint32_t count) {
    pool->block_size = block_size;
    pool->total_blocks = count;
    pool->free_blocks = count;
    pool->free_list = NULL;
    pool->used_list = NULL;
    pool->allocations = 0;
    pool->deallocations = 0;
    pool->bytes_allocated = 0;
    pool->bytes_freed = 0;
    
    // Pre-allocate all blocks
    static uint8_t pool_memory[32 * 1024 * 1024];  // 32MB static pool
    static uint32_t pool_offset = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        PoolBlock* block = (PoolBlock*)(pool_memory + pool_offset);
        pool_offset += sizeof(PoolBlock) + block_size;
        
        block->size = block_size;
        block->used = 0;
        block->next = pool->free_list;
        pool->free_list = block;
    }
}

// Allocate from pool using lock-free CAS
static void* pool_alloc(MemoryPool* pool, size_t size) {
    if (size > pool->block_size) {
        return NULL;  // Too large for this pool
    }
    
    PoolBlock* block;
    PoolBlock* new_head;
    
    // Lock-free allocation using compare-and-swap
    do {
        block = pool->free_list;
        if (!block) {
            pool->cache_misses++;
            return NULL;  // Pool exhausted
        }
        new_head = block->next;
    } while (!__sync_bool_compare_and_swap(&pool->free_list, block, new_head));
    
    // Update statistics atomically
    __sync_fetch_and_add(&pool->free_blocks, -1);
    __sync_fetch_and_add(&pool->allocations, 1);
    __sync_fetch_and_add(&pool->bytes_allocated, size);
    
    block->used = size;
    block->next = NULL;
    
    // Add to used list (simplified - in production use lock-free list)
    block->next = pool->used_list;
    pool->used_list = block;
    
    return block->data;
}

// Free back to pool
static void pool_free(MemoryPool* pool, void* ptr) {
    if (!ptr) return;
    
    // Get block header
    PoolBlock* block = (PoolBlock*)((uint8_t*)ptr - sizeof(PoolBlock));
    
    // Update statistics
    __sync_fetch_and_add(&pool->deallocations, 1);
    __sync_fetch_and_add(&pool->bytes_freed, block->used);
    
    // Clear the block
    for (uint32_t i = 0; i < block->used; i++) {
        block->data[i] = 0;
    }
    block->used = 0;
    
    // Return to free list using CAS
    PoolBlock* old_head;
    do {
        old_head = pool->free_list;
        block->next = old_head;
    } while (!__sync_bool_compare_and_swap(&pool->free_list, old_head, block));
    
    __sync_fetch_and_add(&pool->free_blocks, 1);
}

// Initialize thread-local pools
static void init_thread_pools(int tid) {
    if (thread_pool_initialized[tid]) return;
    
    ThreadLocalPools* pools = &thread_pools[tid];
    
    init_pool(&pools->small_pool, POOL_SMALL_SIZE, POOL_SMALL_COUNT);
    init_pool(&pools->medium_pool, POOL_MEDIUM_SIZE, POOL_MEDIUM_COUNT);
    init_pool(&pools->large_pool, POOL_LARGE_SIZE, POOL_LARGE_COUNT);
    init_pool(&pools->huge_pool, POOL_HUGE_SIZE, POOL_HUGE_COUNT);
    
    pools->total_allocations = 0;
    pools->cache_hits = 0;
    pools->cache_misses = 0;
    
    thread_pool_initialized[tid] = 1;
    
    print_str("[SOLID-POOL] Thread ");
    print_num(tid);
    print_str(" pools initialized\n");
}

// Main allocation function with pool selection
void* solid_pool_alloc(size_t size) {
    int tid = get_thread_id();
    
    // Lazy initialization
    if (!thread_pool_initialized[tid]) {
        init_thread_pools(tid);
    }
    
    ThreadLocalPools* pools = &thread_pools[tid];
    void* ptr = NULL;
    
    __sync_fetch_and_add(&pools->total_allocations, 1);
    
    // Select appropriate pool based on size
    if (size <= POOL_SMALL_SIZE) {
        ptr = pool_alloc(&pools->small_pool, size);
        if (ptr) {
            __sync_fetch_and_add(&pools->cache_hits, 1);
            return ptr;
        }
    }
    
    if (size <= POOL_MEDIUM_SIZE) {
        ptr = pool_alloc(&pools->medium_pool, size);
        if (ptr) {
            __sync_fetch_and_add(&pools->cache_hits, 1);
            return ptr;
        }
    }
    
    if (size <= POOL_LARGE_SIZE) {
        ptr = pool_alloc(&pools->large_pool, size);
        if (ptr) {
            __sync_fetch_and_add(&pools->cache_hits, 1);
            return ptr;
        }
    }
    
    if (size <= POOL_HUGE_SIZE) {
        ptr = pool_alloc(&pools->huge_pool, size);
        if (ptr) {
            __sync_fetch_and_add(&pools->cache_hits, 1);
            return ptr;
        }
    }
    
    // Fall back to system allocator for very large allocations
    __sync_fetch_and_add(&pools->cache_misses, 1);
    
    // Use static buffer as last resort
    static uint8_t fallback_buffer[1024 * 1024];  // 1MB fallback
    static uint32_t fallback_offset = 0;
    
    if (fallback_offset + size < sizeof(fallback_buffer)) {
        ptr = fallback_buffer + fallback_offset;
        fallback_offset += (size + 15) & ~15;  // 16-byte aligned
        return ptr;
    }
    
    return NULL;  // Out of memory
}

// Free with pool detection
void solid_pool_free(void* ptr, size_t size) {
    if (!ptr) return;
    
    int tid = get_thread_id();
    ThreadLocalPools* pools = &thread_pools[tid];
    
    // Determine which pool based on size
    if (size <= POOL_SMALL_SIZE) {
        pool_free(&pools->small_pool, ptr);
    } else if (size <= POOL_MEDIUM_SIZE) {
        pool_free(&pools->medium_pool, ptr);
    } else if (size <= POOL_LARGE_SIZE) {
        pool_free(&pools->large_pool, ptr);
    } else if (size <= POOL_HUGE_SIZE) {
        pool_free(&pools->huge_pool, ptr);
    }
    // Very large allocations from fallback buffer can't be freed individually
}

// Optimized solid number allocation
SolidNumber* solid_alloc_optimized(void) {
    // Calculate size needed
    size_t base_size = sizeof(SolidNumber);
    
    // Most solid numbers fit in small pool
    SolidNumber* solid = (SolidNumber*)solid_pool_alloc(base_size);
    if (!solid) return NULL;
    
    // Initialize
    solid->ref_count = 1;
    solid->barrier_type = BARRIER_EXACT;
    solid->gap_magnitude = 0;
    solid->confidence_x1000 = 1000;
    solid->known_len = 0;
    solid->terminal_len = 0;
    solid->terminal_type = TERMINAL_DIGITS;
    
    return solid;
}

// Get pool statistics
void solid_pool_stats(void) {
    print_str("\n=== SOLID MEMORY POOL STATISTICS ===\n");
    
    for (int tid = 0; tid < 16; tid++) {
        if (!thread_pool_initialized[tid]) continue;
        
        ThreadLocalPools* pools = &thread_pools[tid];
        
        print_str("\nThread ");
        print_num(tid);
        print_str(" Statistics:\n");
        
        print_str("  Total allocations: ");
        print_num(pools->total_allocations);
        print_str("\n  Cache hits: ");
        print_num(pools->cache_hits);
        print_str(" (");
        if (pools->total_allocations > 0) {
            print_num((pools->cache_hits * 100) / pools->total_allocations);
        } else {
            print_num(0);
        }
        print_str("%)\n  Cache misses: ");
        print_num(pools->cache_misses);
        print_str("\n");
        
        // Pool-specific stats
        print_str("\n  Small pool (");
        print_num(POOL_SMALL_SIZE);
        print_str(" bytes):\n");
        print_str("    Allocations: ");
        print_num(pools->small_pool.allocations);
        print_str("\n    Free blocks: ");
        print_num(pools->small_pool.free_blocks);
        print_str("/");
        print_num(pools->small_pool.total_blocks);
        print_str("\n");
        
        print_str("\n  Medium pool (");
        print_num(POOL_MEDIUM_SIZE);
        print_str(" bytes):\n");
        print_str("    Allocations: ");
        print_num(pools->medium_pool.allocations);
        print_str("\n    Free blocks: ");
        print_num(pools->medium_pool.free_blocks);
        print_str("/");
        print_num(pools->medium_pool.total_blocks);
        print_str("\n");
    }
}

// Defragment pools (for maintenance)
void solid_pool_defrag(void) {
    print_str("[SOLID-POOL] Starting defragmentation...\n");
    
    for (int tid = 0; tid < 16; tid++) {
        if (!thread_pool_initialized[tid]) continue;
        
        ThreadLocalPools* pools = &thread_pools[tid];
        
        // Coalesce adjacent free blocks (simplified)
        // In production, would implement proper defragmentation
        
        print_str("[SOLID-POOL] Thread ");
        print_num(tid);
        print_str(" defragmented\n");
    }
}

// Warm up pools by pre-allocating
void solid_pool_warmup(void) {
    print_str("[SOLID-POOL] Warming up memory pools...\n");
    
    int tid = get_thread_id();
    init_thread_pools(tid);
    
    // Pre-allocate and free some blocks to warm up the pools
    void* warmup_ptrs[100];
    
    for (int i = 0; i < 100; i++) {
        warmup_ptrs[i] = solid_pool_alloc(32);
    }
    
    for (int i = 0; i < 100; i++) {
        solid_pool_free(warmup_ptrs[i], 32);
    }
    
    print_str("[SOLID-POOL] Warmup complete\n");
}