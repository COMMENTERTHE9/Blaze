// Timeline synchronization runtime for Blaze
// Implements fixed points and permanent flow control

#include "blaze_internals.h"

// Fixed point table - located at fixed memory address
#define FIXEDPOINT_TABLE_ADDR 0x600000
#define MAX_FIXEDPOINTS 256
#define MAX_WAITING_TIMELINES 64

// Flow control table
#define FLOW_TABLE_ADDR 0x610000  
#define MAX_FLOW_TIMELINES 128

// Fixed point structure in memory
typedef struct {
    uint64_t fixpoint_id;
    char name[32];
    uint64_t waiting_mask;           // Bitmask of waiting timelines
    uint64_t arrived_mask;           // Bitmask of arrived timelines
    uint64_t required_mask;          // Bitmask of required timelines
    void* timeline_data[MAX_WAITING_TIMELINES];
    uint8_t active;
} FixedPoint;

// Flow control structure
typedef struct {
    uint64_t timeline_id;
    uint8_t flow_type;              // 0=PERMANENT, 1=RATE_LIMITED
    uint64_t rate_hz;               // Execution rate (0 = unlimited)
    uint64_t last_exec_cycles;      // CPU cycles at last execution
    uint64_t next_exec_cycles;      // When to execute next
    void* execution_context;        // Saved context for resumption
    uint8_t active;                 // Is flow active
    uint8_t paused;                 // Is flow paused
} FlowControl;

// Get CPU cycle count for timing
static inline uint64_t get_cpu_cycles() {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

// Forward declarations
void release_fixedpoint(uint64_t fixpoint_id);
void block_timeline(uint64_t timeline_id);
void unblock_timeline(uint64_t timeline_id);

// Initialize fixed point system
void init_fixedpoint_system() {
    FixedPoint* table = (FixedPoint*)FIXEDPOINT_TABLE_ADDR;
    for (int i = 0; i < MAX_FIXEDPOINTS; i++) {
        table[i].active = 0;
        table[i].waiting_mask = 0;
        table[i].arrived_mask = 0;
        table[i].required_mask = 0;
    }
}

// Initialize flow control system
void init_flow_system() {
    FlowControl* table = (FlowControl*)FLOW_TABLE_ADDR;
    for (int i = 0; i < MAX_FLOW_TIMELINES; i++) {
        table[i].active = 0;
        table[i].flow_type = 0;
        table[i].rate_hz = 0;
    }
}

// Register a fixed point
uint64_t register_fixedpoint(const char* name) {
    FixedPoint* table = (FixedPoint*)FIXEDPOINT_TABLE_ADDR;
    
    // Find free slot
    for (int i = 0; i < MAX_FIXEDPOINTS; i++) {
        if (!table[i].active) {
            table[i].active = 1;
            table[i].fixpoint_id = i;
            
            // Copy name
            int j = 0;
            while (name[j] && j < 31) {
                table[i].name[j] = name[j];
                j++;
            }
            table[i].name[j] = '\0';
            
            return i;
        }
    }
    return 0xFFFFFFFFFFFFFFFF; // No free slots
}

// Timeline arrives at fixed point
void timeline_arrive_fixedpoint(uint64_t fixpoint_id, uint64_t timeline_id, void* timeline_data) {
    FixedPoint* fp = &((FixedPoint*)FIXEDPOINT_TABLE_ADDR)[fixpoint_id];
    
    if (!fp->active) return;
    
    // Mark timeline as arrived
    fp->arrived_mask |= (1ULL << timeline_id);
    fp->timeline_data[timeline_id] = timeline_data;
    
    // Check if all required timelines have arrived
    if ((fp->arrived_mask & fp->required_mask) == fp->required_mask) {
        // Release all waiting timelines
        release_fixedpoint(fixpoint_id);
    }
    else {
        // Block this timeline
        block_timeline(timeline_id);
    }
}

// Release all timelines at fixed point
void release_fixedpoint(uint64_t fixpoint_id) {
    FixedPoint* fp = &((FixedPoint*)FIXEDPOINT_TABLE_ADDR)[fixpoint_id];
    
    // Wake up all waiting timelines
    for (int i = 0; i < MAX_WAITING_TIMELINES; i++) {
        if (fp->arrived_mask & (1ULL << i)) {
            unblock_timeline(i);
        }
    }
    
    // Reset for next synchronization
    fp->arrived_mask = 0;
}

// Register a permanent timeline
uint64_t register_permanent_timeline(uint64_t timeline_id, uint64_t rate_hz) {
    FlowControl* table = (FlowControl*)FLOW_TABLE_ADDR;
    
    // Find slot for timeline
    for (int i = 0; i < MAX_FLOW_TIMELINES; i++) {
        if (!table[i].active) {
            table[i].active = 1;
            table[i].timeline_id = timeline_id;
            table[i].flow_type = (rate_hz > 0) ? 1 : 0;
            table[i].rate_hz = rate_hz;
            table[i].last_exec_cycles = get_cpu_cycles();
            
            // Calculate next execution time if rate limited
            if (rate_hz > 0) {
                // Assuming ~3GHz CPU for cycle->time conversion
                uint64_t cycles_per_exec = 3000000000ULL / rate_hz;
                table[i].next_exec_cycles = table[i].last_exec_cycles + cycles_per_exec;
            }
            
            return i;
        }
    }
    return 0xFFFFFFFFFFFFFFFF;
}

// Check if permanent timeline should execute
bool should_execute_flow(uint64_t flow_id) {
    FlowControl* flow = &((FlowControl*)FLOW_TABLE_ADDR)[flow_id];
    
    if (!flow->active || flow->paused) return false;
    
    if (flow->flow_type == 0) {
        // Permanent flow - always execute
        return true;
    }
    else {
        // Rate limited - check timing
        uint64_t current_cycles = get_cpu_cycles();
        if (current_cycles >= flow->next_exec_cycles) {
            // Update next execution time
            uint64_t cycles_per_exec = 3000000000ULL / flow->rate_hz;
            flow->next_exec_cycles = current_cycles + cycles_per_exec;
            flow->last_exec_cycles = current_cycles;
            return true;
        }
        return false;
    }
}

// Pause a permanent flow
void pause_flow(uint64_t flow_id) {
    FlowControl* flow = &((FlowControl*)FLOW_TABLE_ADDR)[flow_id];
    flow->paused = 1;
}

// Resume a permanent flow
void resume_flow(uint64_t flow_id, uint64_t new_rate) {
    FlowControl* flow = &((FlowControl*)FLOW_TABLE_ADDR)[flow_id];
    flow->paused = 0;
    
    if (new_rate > 0) {
        flow->rate_hz = new_rate;
        flow->flow_type = 1;
        
        // Recalculate next execution
        uint64_t current_cycles = get_cpu_cycles();
        uint64_t cycles_per_exec = 3000000000ULL / new_rate;
        flow->next_exec_cycles = current_cycles + cycles_per_exec;
    }
}

// Terminate a permanent flow
void terminate_flow(uint64_t flow_id) {
    FlowControl* flow = &((FlowControl*)FLOW_TABLE_ADDR)[flow_id];
    flow->active = 0;
    flow->paused = 0;
}

// Placeholder functions for timeline blocking
// These would be implemented with actual OS/runtime support
void block_timeline(uint64_t timeline_id) {
    // In real implementation: suspend timeline execution
    // For now, just a placeholder
}

void unblock_timeline(uint64_t timeline_id) {
    // In real implementation: resume timeline execution
    // For now, just a placeholder
}