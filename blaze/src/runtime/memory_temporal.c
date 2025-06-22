// TEMPORAL MEMORY MANAGEMENT FOR BLAZE
// Handles memory allocation across past, present, and future zones

#include "blaze_internals.h"

// Forward declare for runtime
struct MemoryPrediction;

// TimeZone enum is already defined in blaze_types.h

// Temporal link between time zones
typedef struct TemporalLink {
    uint32_t var_hash;          // Variable identifier hash
    void* past_addr;            // Address in past zone
    void* present_addr;         // Address in present zone
    void* future_addr;          // Address in future zone
    uint16_t link_count;        // Number of active links
    int32_t temporal_offset;    // Time offset for this link
    struct TemporalLink* next;  // Chain for hash collisions
} TemporalLink;

// Memory zone structure
typedef struct {
    uint8_t* base;              // Base address of zone
    uint32_t size;              // Total size
    uint32_t used;              // Current usage
    uint32_t watermark;         // High water mark
} MemoryZone;

// GAP variable metadata - extended version for internal use
typedef struct GapMetadataExtended {
    uint32_t var_hash;          // Variable identifier
    float confidence_score;     // Current confidence [0.0, 1.0]
    uint8_t missing_data_count; // Number of missing data points
    struct {
        char name[32];          // Missing data item name
        bool is_critical;       // Critical for confidence
    } missing_data[8];          // Up to 8 missing items
    
    // Confidence thresholds
    float migration_threshold;  // When to move from UNKNOWN
    TimeZone target_zone;       // Where to move when confident
    
    struct GapMetadataExtended* next;   // Chain for hash collisions
} GapMetadataExtended;

// Temporal memory management structure
struct TemporalMemory {
    // Memory zones - stack allocated
    MemoryZone past_zone;
    MemoryZone present_zone;
    MemoryZone future_zone;
    MemoryZone unknown_zone;    // GAP variables zone
    
    // Temporal links hash table
    TemporalLink* links[64];    // Hash table of temporal links
    uint16_t link_count;
    
    // GAP metadata hash table
    GapMetadataExtended* gap_data[32];  // GAP variable metadata
    uint16_t gap_count;
    
    // Stack frame info
    void* frame_base;           // Current stack frame base
    int32_t frame_offset;       // Offset from frame base
    
    // GGGX prediction mode
    bool prediction_mode;       // True when predicting, not allocating
    uint32_t predicted_usage;   // Predicted memory usage
};

// Global temporal memory instance (per execution context)
static TemporalMemory g_temporal_mem;

// Initialize temporal memory system
void temporal_memory_init(void* stack_base, uint32_t stack_size) {
    // Clear everything
    for (int i = 0; i < 64; i++) {
        g_temporal_mem.links[i] = NULL;
    }
    for (int i = 0; i < 32; i++) {
        g_temporal_mem.gap_data[i] = NULL;
    }
    
    // Divide stack into temporal zones
    // Layout: [FUTURE | UNKNOWN | PRESENT | PAST | TEMPORAL]
    uint32_t zone_size = stack_size / 5;
    
    g_temporal_mem.future_zone.base = (uint8_t*)stack_base;
    g_temporal_mem.future_zone.size = zone_size;
    g_temporal_mem.future_zone.used = 0;
    
    g_temporal_mem.unknown_zone.base = (uint8_t*)stack_base + zone_size;
    g_temporal_mem.unknown_zone.size = zone_size;
    g_temporal_mem.unknown_zone.used = 0;
    
    g_temporal_mem.present_zone.base = (uint8_t*)stack_base + (zone_size * 2);
    g_temporal_mem.present_zone.size = zone_size;
    g_temporal_mem.present_zone.used = 0;
    
    g_temporal_mem.past_zone.base = (uint8_t*)stack_base + (zone_size * 3);
    g_temporal_mem.past_zone.size = zone_size;
    g_temporal_mem.past_zone.used = 0;
    
    g_temporal_mem.frame_base = stack_base;
    g_temporal_mem.frame_offset = 0;
    g_temporal_mem.link_count = 0;
    g_temporal_mem.gap_count = 0;
    g_temporal_mem.prediction_mode = false;
}

// Hash function for variable names
static uint32_t hash_var_name(const char* name) {
    uint32_t hash = 5381;
    while (*name) {
        hash = ((hash << 5) + hash) + *name++;
    }
    return hash % 64; // Fit in hash table
}

// Allocate memory in specific zone
static void* allocate_in_zone(MemoryZone* zone, uint32_t size, uint32_t alignment) {
    // Align allocation
    uint32_t aligned_used = (zone->used + alignment - 1) & ~(alignment - 1);
    
    // Check space
    if (aligned_used + size > zone->size) {
        return NULL; // Zone full
    }
    
    void* addr = zone->base + aligned_used;
    zone->used = aligned_used + size;
    
    // Update watermark
    if (zone->used > zone->watermark) {
        zone->watermark = zone->used;
    }
    
    return addr;
}

// Allocate temporal variable
void* temporal_alloc_var(const char* name, uint32_t size, TimeZone zone) {
    // In prediction mode, just track usage
    if (g_temporal_mem.prediction_mode) {
        g_temporal_mem.predicted_usage += size;
        return (void*)0x1000; // Fake address
    }
    
    MemoryZone* target_zone = NULL;
    
    switch (zone) {
        case ZONE_FUTURE:
            target_zone = &g_temporal_mem.future_zone;
            break;
        case ZONE_PRESENT:
            target_zone = &g_temporal_mem.present_zone;
            break;
        case ZONE_PAST:
            target_zone = &g_temporal_mem.past_zone;
            break;
        case ZONE_UNKNOWN:
            target_zone = &g_temporal_mem.unknown_zone;
            break;
        default:
            return NULL;
    }
    
    // Allocate with 8-byte alignment
    void* addr = allocate_in_zone(target_zone, size, 8);
    
    // Create temporal link entry
    if (addr && (zone == ZONE_FUTURE || zone == ZONE_PAST)) {
        uint32_t hash_idx = hash_var_name(name);
        
        // Find or create link
        TemporalLink* link = g_temporal_mem.links[hash_idx];
        while (link) {
            if (link->var_hash == hash_var_name(name)) {
                break;
            }
            link = link->next;
        }
        
        if (!link) {
            // Allocate new link (in temporal zone)
            link = (TemporalLink*)allocate_in_zone(&g_temporal_mem.present_zone, 
                                                   sizeof(TemporalLink), 8);
            if (link) {
                link->var_hash = hash_var_name(name);
                link->past_addr = NULL;
                link->present_addr = NULL;
                link->future_addr = NULL;
                link->link_count = 0;
                link->next = g_temporal_mem.links[hash_idx];
                g_temporal_mem.links[hash_idx] = link;
                g_temporal_mem.link_count++;
            }
        }
        
        // Update appropriate address
        if (link) {
            switch (zone) {
                case ZONE_FUTURE:
                    link->future_addr = addr;
                    break;
                case ZONE_PAST:
                    link->past_addr = addr;
                    break;
                case ZONE_PRESENT:
                    link->present_addr = addr;
                    break;
            }
            link->link_count++;
        }
    }
    
    return addr;
}

// Allocate GAP variable in unknown zone
void* temporal_alloc_gap_var(const char* name, uint32_t size, float initial_confidence,
                            float migration_threshold) {
    // First allocate the variable in unknown zone
    void* addr = temporal_alloc_var(name, size, ZONE_UNKNOWN);
    if (!addr) return NULL;
    
    // Create GAP metadata
    uint32_t hash_idx = hash_var_name(name) % 32;
    
    GapMetadataExtended* meta = (GapMetadataExtended*)allocate_in_zone(&g_temporal_mem.present_zone,
                                                       sizeof(GapMetadataExtended), 8);
    if (!meta) return addr; // Variable allocated but no metadata
    
    // Initialize metadata
    meta->var_hash = hash_var_name(name);
    meta->confidence_score = initial_confidence;
    meta->missing_data_count = 0;
    meta->migration_threshold = migration_threshold;
    meta->target_zone = ZONE_PRESENT; // Default target
    
    // Add to hash table
    meta->next = g_temporal_mem.gap_data[hash_idx];
    g_temporal_mem.gap_data[hash_idx] = meta;
    g_temporal_mem.gap_count++;
    
    return addr;
}

// Add missing data item to GAP variable
void temporal_gap_add_missing(const char* var_name, const char* missing_item, bool is_critical) {
    uint32_t hash_idx = hash_var_name(var_name) % 32;
    uint32_t var_hash = hash_var_name(var_name);
    
    // Find GAP metadata
    GapMetadataExtended* meta = g_temporal_mem.gap_data[hash_idx];
    while (meta) {
        if (meta->var_hash == var_hash) {
            if (meta->missing_data_count < 8) {
                // Copy missing item name
                uint32_t i = 0;
                while (missing_item[i] && i < 31) {
                    meta->missing_data[meta->missing_data_count].name[i] = missing_item[i];
                    i++;
                }
                meta->missing_data[meta->missing_data_count].name[i] = '\0';
                meta->missing_data[meta->missing_data_count].is_critical = is_critical;
                meta->missing_data_count++;
                
                // Reduce confidence for critical missing data
                if (is_critical) {
                    meta->confidence_score *= 0.8;
                }
            }
            return;
        }
        meta = meta->next;
    }
}

// Update GAP variable confidence
void temporal_gap_update_confidence(const char* var_name, float new_confidence) {
    uint32_t hash_idx = hash_var_name(var_name) % 32;
    uint32_t var_hash = hash_var_name(var_name);
    
    GapMetadataExtended* meta = g_temporal_mem.gap_data[hash_idx];
    while (meta) {
        if (meta->var_hash == var_hash) {
            meta->confidence_score = new_confidence;
            
            // Check if ready to migrate
            if (new_confidence >= meta->migration_threshold) {
                // TODO: Implement zone migration
                // This would move the variable from UNKNOWN to target zone
            }
            return;
        }
        meta = meta->next;
    }
}

// Create temporal link between zones
void temporal_create_link(const char* var_name, TimeZone from_zone, TimeZone to_zone,
                         int32_t temporal_offset) {
    uint32_t hash_idx = hash_var_name(var_name);
    uint32_t var_hash = hash_var_name(var_name);
    
    // Find existing link
    TemporalLink* link = g_temporal_mem.links[hash_idx];
    while (link) {
        if (link->var_hash == var_hash) {
            link->temporal_offset = temporal_offset;
            return;
        }
        link = link->next;
    }
    
    // Create new link if not found
    link = (TemporalLink*)allocate_in_zone(&g_temporal_mem.present_zone,
                                          sizeof(TemporalLink), 8);
    if (link) {
        link->var_hash = var_hash;
        link->temporal_offset = temporal_offset;
        link->link_count = 1;
        link->next = g_temporal_mem.links[hash_idx];
        g_temporal_mem.links[hash_idx] = link;
    }
}

// Resolve variable access across time zones
void* temporal_resolve_var(const char* var_name, bool needs_future_value) {
    uint32_t hash_idx = hash_var_name(var_name);
    uint32_t var_hash = hash_var_name(var_name);
    
    // Check GAP variables first (they might need computation)
    uint32_t gap_idx = var_hash % 32;
    GapMetadataExtended* gap_meta = g_temporal_mem.gap_data[gap_idx];
    while (gap_meta) {
        if (gap_meta->var_hash == var_hash) {
            // Found GAP variable - check confidence
            if (gap_meta->confidence_score < gap_meta->migration_threshold) {
                // Still in unknown zone - trigger GAP computation
                // For now, return NULL to indicate computation needed
                return NULL;
            }
            break;
        }
        gap_meta = gap_meta->next;
    }
    
    // Find temporal link
    TemporalLink* link = g_temporal_mem.links[hash_idx];
    while (link) {
        if (link->var_hash == var_hash) {
            // Return appropriate address based on context
            if (needs_future_value && link->future_addr) {
                return link->future_addr;
            } else if (link->present_addr) {
                return link->present_addr;
            } else if (link->past_addr) {
                return link->past_addr;
            }
        }
        link = link->next;
    }
    
    return NULL; // Not found
}

// Get GAP variable metadata (internal)
static GapMetadataExtended* temporal_get_gap_metadata_internal(const char* var_name) {
    uint32_t hash_idx = hash_var_name(var_name) % 32;
    uint32_t var_hash = hash_var_name(var_name);
    
    GapMetadataExtended* meta = g_temporal_mem.gap_data[hash_idx];
    while (meta) {
        if (meta->var_hash == var_hash) {
            return meta;
        }
        meta = meta->next;
    }
    
    return NULL;
}

// Get GAP variable metadata (public interface)
GapMetadata* temporal_get_gap_metadata(const char* var_name) {
    GapMetadataExtended* ext = temporal_get_gap_metadata_internal(var_name);
    if (ext) {
        // Return pointer to the simplified metadata portion
        return (GapMetadata*)&ext->confidence_score;
    }
    return NULL;
}

// Migrate GAP variable from unknown zone to target zone
bool temporal_gap_migrate(const char* var_name) {
    GapMetadataExtended* meta = temporal_get_gap_metadata_internal(var_name);
    if (!meta || meta->confidence_score < meta->migration_threshold) {
        return false; // Not ready to migrate
    }
    
    // Find variable in unknown zone
    // TODO: Implement actual memory movement
    // This would involve:
    // 1. Finding the variable's address in unknown zone
    // 2. Allocating space in target zone
    // 3. Copying data
    // 4. Updating temporal links
    // 5. Freeing space in unknown zone
    
    return true;
}

// 4D Array temporal allocation
struct Array4D {
    uint32_t dimensions[4];     // [x, y, z, time]
    void* base_addr;            // Base address of array data
    uint32_t element_size;      // Size of each element
    
    // Temporal slices
    struct {
        int32_t time_index;     // Time coordinate
        void* slice_addr;       // Address of this time slice
    } time_slices[16];          // Up to 16 time slices
    uint8_t slice_count;
};

// Allocate 4D array with temporal support
Array4D* temporal_alloc_array4d(uint32_t x, uint32_t y, uint32_t z, uint32_t t,
                               uint32_t elem_size) {
    // Allocate array descriptor
    Array4D* arr = (Array4D*)temporal_alloc_var("_array4d", sizeof(Array4D), ZONE_PRESENT);
    if (!arr) return NULL;
    
    // Set dimensions
    arr->dimensions[0] = x;
    arr->dimensions[1] = y;
    arr->dimensions[2] = z;
    arr->dimensions[3] = t;
    arr->element_size = elem_size;
    
    // Calculate total size
    uint32_t slice_size = x * y * z * elem_size;
    uint32_t total_size = slice_size * t;
    
    // For large arrays, allocate time slices separately
    if (t > 1 && total_size > 4096) {
        // Allocate each time slice in appropriate zone
        arr->slice_count = t > 16 ? 16 : t;
        
        for (uint8_t i = 0; i < arr->slice_count; i++) {
            // Future slices in future zone, past in past zone
            TimeZone zone = (i < t/2) ? ZONE_PAST : ZONE_FUTURE;
            
            arr->time_slices[i].time_index = i;
            arr->time_slices[i].slice_addr = temporal_alloc_var("_slice", 
                                                               slice_size, zone);
            
            if (!arr->time_slices[i].slice_addr) {
                return NULL; // Allocation failed
            }
        }
        
        arr->base_addr = arr->time_slices[0].slice_addr;
    } else {
        // Small array - allocate contiguously
        arr->base_addr = temporal_alloc_var("_array_data", total_size, ZONE_PRESENT);
        arr->slice_count = 0;
    }
    
    return arr;
}

// Access 4D array element with temporal awareness
void* temporal_array4d_access(Array4D* arr, uint32_t x, uint32_t y, uint32_t z, uint32_t t) {
    // Bounds check
    if (x >= arr->dimensions[0] || y >= arr->dimensions[1] ||
        z >= arr->dimensions[2] || t >= arr->dimensions[3]) {
        return NULL;
    }
    
    // Calculate offset
    uint32_t slice_size = arr->dimensions[0] * arr->dimensions[1] * arr->dimensions[2];
    uint32_t slice_offset = (x + y * arr->dimensions[0] + 
                           z * arr->dimensions[0] * arr->dimensions[1]) * arr->element_size;
    
    // Handle temporal slicing
    if (arr->slice_count > 0) {
        // Find appropriate time slice
        for (uint8_t i = 0; i < arr->slice_count; i++) {
            if (arr->time_slices[i].time_index == t) {
                return (uint8_t*)arr->time_slices[i].slice_addr + slice_offset;
            }
        }
        return NULL; // Time slice not found
    } else {
        // Contiguous allocation
        uint32_t full_offset = t * slice_size * arr->element_size + slice_offset;
        return (uint8_t*)arr->base_addr + full_offset;
    }
}

// GGGX Memory prediction
// MemoryPrediction is defined in blaze_types.h

// Predict memory usage for GGGX
MemoryPrediction temporal_predict_memory(ASTNode* nodes, uint16_t node_idx, 
                                       SymbolTable* symbols) {
    MemoryPrediction pred = {0};
    
    // Enter prediction mode
    g_temporal_mem.prediction_mode = true;
    g_temporal_mem.predicted_usage = 0;
    
    // Simulate memory allocations
    for (uint16_t i = 0; i < symbols->symbol_count; i++) {
        Symbol* sym = &symbols->symbols[i];
        
        switch (sym->type) {
            case SYMBOL_VARIABLE:
            case SYMBOL_TEMPORAL:
                pred.stack_usage += 8; // 64-bit variable
                if (sym->data.var.is_temporal) {
                    pred.temporal_links += sizeof(TemporalLink);
                    pred.future_zone_usage += 8;
                }
                break;
                
            case SYMBOL_ARRAY_4D:
                {
                    uint32_t total = 1;
                    for (int j = 0; j < 4; j++) {
                        total *= sym->data.array_4d.dimensions[j];
                    }
                    pred.array_usage += total * 8; // Assume 8-byte elements
                }
                break;
        }
    }
    
    // Check for overflow
    uint32_t total_usage = pred.stack_usage + pred.temporal_links + 
                          pred.future_zone_usage + pred.array_usage;
    pred.will_overflow = (total_usage > 65536); // 64KB stack limit
    
    // Exit prediction mode
    g_temporal_mem.prediction_mode = false;
    
    return pred;
}

// Reset temporal zones (for new execution context)
void temporal_memory_reset(void) {
    g_temporal_mem.past_zone.used = 0;
    g_temporal_mem.present_zone.used = 0;
    g_temporal_mem.future_zone.used = 0;
    g_temporal_mem.unknown_zone.used = 0;
    
    // Clear temporal links
    for (int i = 0; i < 64; i++) {
        g_temporal_mem.links[i] = NULL;
    }
    g_temporal_mem.link_count = 0;
    
    // Clear GAP metadata
    for (int i = 0; i < 32; i++) {
        g_temporal_mem.gap_data[i] = NULL;
    }
    g_temporal_mem.gap_count = 0;
}

// Get memory statistics
void temporal_memory_stats(uint32_t* past_used, uint32_t* present_used, 
                          uint32_t* future_used, uint16_t* link_count) {
    *past_used = g_temporal_mem.past_zone.used;
    *present_used = g_temporal_mem.present_zone.used;
    *future_used = g_temporal_mem.future_zone.used;
    *link_count = g_temporal_mem.link_count;
}

// Get extended memory statistics including unknown zone
void temporal_memory_stats_extended(uint32_t* past_used, uint32_t* present_used, 
                                   uint32_t* future_used, uint32_t* unknown_used,
                                   uint16_t* link_count, uint16_t* gap_count) {
    *past_used = g_temporal_mem.past_zone.used;
    *present_used = g_temporal_mem.present_zone.used;
    *future_used = g_temporal_mem.future_zone.used;
    *unknown_used = g_temporal_mem.unknown_zone.used;
    *link_count = g_temporal_mem.link_count;
    *gap_count = g_temporal_mem.gap_count;
}