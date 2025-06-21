// Timeline bounce mechanism implementation for Blaze
// Handles timeline collisions with bounce, merge, and queue strategies

#include "blaze_internals.h"

// Timeline collision resolution strategies
typedef enum {
    COLLISION_BOUNCE,   // Bounce one timeline away
    COLLISION_MERGE,    // Merge both timelines into one
    COLLISION_QUEUE     // Queue timelines for sequential processing
} CollisionStrategy;

// Timeline state for collision detection
typedef struct {
    uint16_t timeline_id;
    uint64_t target_address;
    uint32_t priority;
    bool is_active;
} TimelineState;

// Timeline collision info
typedef struct {
    TimelineState* timeline_a;
    TimelineState* timeline_b;
    uint64_t collision_address;
    CollisionStrategy strategy;
} TimelineCollision;

// Check if two timelines are targeting the same memory state
bool detect_timeline_collision(TimelineState* a, TimelineState* b) {
    if (!a->is_active || !b->is_active) return false;
    return a->target_address == b->target_address;
}

// Bounce a timeline to an alternate state
void bounce_timeline(TimelineState* timeline, uint64_t alternate_address) {
    // Store original target for potential recovery
    uint64_t original = timeline->target_address;
    
    // Redirect to alternate address
    timeline->target_address = alternate_address;
    
    // Log the bounce event for debugging
    print_str("Timeline ");
    print_num(timeline->timeline_id);
    print_str(" bounced from 0x");
    print_num(original);
    print_str(" to 0x");
    print_num(alternate_address);
    print_str("\n");
}

// Merge two timeline states into one
void merge_timeline_states(TimelineState* primary, TimelineState* secondary) {
    // For now, we'll implement a simple priority-based merge
    // In a full implementation, this would:
    // 1. Combine variable values (average, max, min, or custom)
    // 2. Merge memory states
    // 3. Combine execution contexts
    
    // The higher priority timeline dominates
    if (secondary->priority > primary->priority) {
        // Swap roles
        TimelineState temp = *primary;
        *primary = *secondary;
        *secondary = temp;
    }
    
    // Secondary timeline is absorbed
    secondary->is_active = false;
    
    print_str("Timeline ");
    print_num(secondary->timeline_id);
    print_str(" merged into timeline ");
    print_num(primary->timeline_id);
    print_str("\n");
}

// Queue a timeline for later processing
typedef struct TimelineQueueNode {
    TimelineState timeline;
    struct TimelineQueueNode* next;
} TimelineQueueNode;

typedef struct {
    TimelineQueueNode* head;
    TimelineQueueNode* tail;
    uint32_t count;
} TimelineQueue;

static TimelineQueue global_queue = {NULL, NULL, 0};

void queue_timeline(TimelineState* timeline) {
    // In a real implementation, this would allocate from a pool
    // For now, we'll just log the queueing
    print_str("Timeline ");
    print_num(timeline->timeline_id);
    print_str(" queued for sequential processing\n");
    
    timeline->is_active = false; // Deactivate while queued
    global_queue.count++;
}

// Process next timeline in queue
TimelineState* dequeue_timeline(void) {
    if (global_queue.count == 0) return NULL;
    
    // In real implementation, would remove from linked list
    global_queue.count--;
    
    print_str("Dequeued timeline for processing\n");
    return NULL; // Placeholder
}

// Main collision resolution function
void resolve_timeline_collision(TimelineCollision* collision) {
    switch (collision->strategy) {
        case COLLISION_BOUNCE: {
            // Determine which timeline to bounce (lower priority)
            TimelineState* to_bounce = collision->timeline_a->priority < collision->timeline_b->priority
                ? collision->timeline_a : collision->timeline_b;
            
            // Find alternate address (in real impl, would search for free states)
            uint64_t alternate = collision->collision_address + 0x1000;
            
            bounce_timeline(to_bounce, alternate);
            break;
        }
        
        case COLLISION_MERGE: {
            merge_timeline_states(collision->timeline_a, collision->timeline_b);
            break;
        }
        
        case COLLISION_QUEUE: {
            // Queue the lower priority timeline
            TimelineState* to_queue = collision->timeline_a->priority < collision->timeline_b->priority
                ? collision->timeline_a : collision->timeline_b;
            
            queue_timeline(to_queue);
            break;
        }
    }
}

// Parse recv._merg syntax
bool parse_recv_merge(const char* input, uint32_t pos, uint32_t len) {
    if (pos + 10 < len) {
        return match_string(input, pos, len, "recv._merg");
    }
    return false;
}

// Parse recv._queue syntax
bool parse_recv_queue(const char* input, uint32_t pos, uint32_t len) {
    if (pos + 11 < len) {
        return match_string(input, pos, len, "recv._queue");
    }
    return false;
}

// Runtime timeline collision handler
void handle_timeline_operation(uint16_t timeline_id, uint64_t target_addr, uint32_t priority) {
    static TimelineState active_timelines[16];
    static uint32_t timeline_count = 0;
    
    TimelineState new_timeline = {
        .timeline_id = timeline_id,
        .target_address = target_addr,
        .priority = priority,
        .is_active = true
    };
    
    // Check for collisions with existing timelines
    for (uint32_t i = 0; i < timeline_count; i++) {
        if (detect_timeline_collision(&new_timeline, &active_timelines[i])) {
            TimelineCollision collision = {
                .timeline_a = &new_timeline,
                .timeline_b = &active_timelines[i],
                .collision_address = target_addr,
                .strategy = COLLISION_BOUNCE // Default strategy
            };
            
            resolve_timeline_collision(&collision);
            break;
        }
    }
    
    // Add timeline to active set if still active
    if (new_timeline.is_active && timeline_count < 16) {
        active_timelines[timeline_count++] = new_timeline;
    }
}