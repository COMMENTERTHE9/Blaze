// 4D ARRAY IMPLEMENTATION FOR BLAZE
// Direct memory mapping with temporal zone integration

#include "blaze_internals.h"
#include "array4d_types.h"

// Array4D and Array4DLayout are defined in array4d_types.h

// Helper: Calculate flat index from coordinates
size_t get_flat_index(Array4D* arr, int x, int y, int z, int t) {
    return t * arr->strides[3] + z * arr->strides[2] + 
           y * arr->strides[1] + x * arr->strides[0];
}

// Helper: Test bit in presence map
bool test_bit(uint8_t* map, size_t bit_idx) {
    size_t byte_idx = bit_idx / 8;
    uint8_t bit_mask = 1 << (bit_idx % 8);
    return (map[byte_idx] & bit_mask) != 0;
}

// Helper: Set bit in presence map
static inline void set_bit(uint8_t* map, size_t bit_idx) {
    size_t byte_idx = bit_idx / 8;
    uint8_t bit_mask = 1 << (bit_idx % 8);
    map[byte_idx] |= bit_mask;
}

// Helper: Clear bit in presence map
static inline void clear_bit(uint8_t* map, size_t bit_idx) {
    size_t byte_idx = bit_idx / 8;
    uint8_t bit_mask = 1 << (bit_idx % 8);
    map[byte_idx] &= ~bit_mask;
}

// Ultra-fast direct memory offset calculation
static inline size_t get_memory_offset(Array4D* arr, int x, int y, int z, int t) {
    return x * arr->strides[0] + y * arr->strides[1] + 
           z * arr->strides[2] + t * arr->strides[3];
}

// Direct memory access - BLAZING FAST!
void* get_cell_ptr(Array4D* arr, int x, int y, int z, int t) {
    // Bounds checking
    if (x < 0 || x >= arr->dimensions[0] ||
        y < 0 || y >= arr->dimensions[1] ||
        z < 0 || z >= arr->dimensions[2] ||
        t < 0 || t >= arr->dimensions[3]) {
        return NULL;
    }
    
    size_t offset = get_memory_offset(arr, x, y, z, t);
    
    if (arr->layout_type == LAYOUT_CONTIGUOUS) {
        return (uint8_t*)arr->base_memory + offset;
    } else {
        // For sliced layout - calculate which slice and offset within
        size_t slice_size = arr->strides[3];
        size_t slice_offset = offset % slice_size;
        return (uint8_t*)arr->time_slice_ptrs[t] + slice_offset;
    }
}

// Initialize 4D array
Array4D* array4d_create(uint32_t x, uint32_t y, uint32_t z, uint32_t t, 
                       size_t elem_size) {
    // Allocate array structure
    Array4D* arr = (Array4D*)temporal_alloc_var("array4d", sizeof(Array4D), ZONE_PRESENT);
    if (!arr) return NULL;
    
    // Set dimensions
    arr->dimensions[0] = x;
    arr->dimensions[1] = y;
    arr->dimensions[2] = z;
    arr->dimensions[3] = t;
    arr->element_size = elem_size;
    
    // Pre-compute strides for fast access
    arr->strides[0] = elem_size;
    arr->strides[1] = x * elem_size;
    arr->strides[2] = x * y * elem_size;
    arr->strides[3] = x * y * z * elem_size;
    
    // Calculate total size
    arr->total_size = x * y * z * t * elem_size;
    arr->gap_stats.total_cells = x * y * z * t;
    
    // Decision: contiguous vs sliced based on size
    const size_t SLICE_THRESHOLD = 10 * 1024 * 1024; // 10MB
    
    if (arr->total_size < SLICE_THRESHOLD && t <= 16) {
        // Small array - use contiguous memory
        arr->layout_type = LAYOUT_CONTIGUOUS;
        arr->base_memory = temporal_alloc_var("array_data", arr->total_size, ZONE_PRESENT);
        if (!arr->base_memory) {
            return NULL;
        }
        arr->time_slice_ptrs = NULL;
        arr->slice_zones = NULL;
    } else {
        // Large array - distribute time slices across zones
        arr->layout_type = LAYOUT_TEMPORAL_SLICED;
        arr->base_memory = NULL;
        
        // Allocate slice pointer arrays
        arr->time_slice_ptrs = (void**)temporal_alloc_var("slice_ptrs", 
                                                          t * sizeof(void*), 
                                                          ZONE_PRESENT);
        arr->slice_zones = (TimeZone*)temporal_alloc_var("slice_zones",
                                                         t * sizeof(TimeZone),
                                                         ZONE_PRESENT);
        if (!arr->time_slice_ptrs || !arr->slice_zones) {
            return NULL;
        }
        
        // Allocate time slices in appropriate zones
        size_t slice_size = arr->strides[3];
        for (uint32_t i = 0; i < t; i++) {
            // Distribute across temporal zones
            TimeZone zone;
            if (i == 0) {
                zone = ZONE_PAST;        // First slice in past
            } else if (i == t - 1) {
                zone = ZONE_FUTURE;      // Last slice in future
            } else if (i < t / 3) {
                zone = ZONE_PAST;        // Early slices in past
            } else if (i > 2 * t / 3) {
                zone = ZONE_FUTURE;      // Late slices in future
            } else {
                zone = ZONE_PRESENT;     // Middle slices in present
            }
            
            arr->slice_zones[i] = zone;
            arr->time_slice_ptrs[i] = temporal_alloc_var("time_slice", slice_size, zone);
            
            if (!arr->time_slice_ptrs[i]) {
                // Cleanup on failure
                for (uint32_t j = 0; j < i; j++) {
                    // Note: In real implementation, would need temporal_free
                }
                return NULL;
            }
        }
    }
    
    // Initialize data presence bitmap
    arr->presence_map_size = (arr->gap_stats.total_cells + 7) / 8; // Round up
    arr->data_presence_map = (uint8_t*)temporal_alloc_var("presence_map", 
                                                          arr->presence_map_size,
                                                          ZONE_PRESENT);
    if (arr->data_presence_map) {
        // Clear all bits - no data present initially
        for (uint32_t i = 0; i < arr->presence_map_size; i++) {
            arr->data_presence_map[i] = 0;
        }
    }
    
    // Optional: Initialize confidence map (only if needed)
    arr->confidence_map = NULL; // Allocate on demand
    
    // Set temporal state
    arr->current_time_index = 0;
    arr->gap_stats.missing_count = arr->gap_stats.total_cells; // All missing initially
    arr->gap_stats.overall_confidence = 0.0;
    
    return arr;
}

// Set value with data presence tracking
void array4d_set(Array4D* arr, int x, int y, int z, int t, void* value) {
    void* cell = get_cell_ptr(arr, x, y, z, t);
    if (!cell) return; // Out of bounds
    
    // Copy value based on element size
    switch (arr->element_size) {
        case 1: *(uint8_t*)cell = *(uint8_t*)value; break;
        case 2: *(uint16_t*)cell = *(uint16_t*)value; break;
        case 4: *(uint32_t*)cell = *(uint32_t*)value; break;
        case 8: *(uint64_t*)cell = *(uint64_t*)value; break;
        default:
            // Generic memcpy for other sizes
            for (size_t i = 0; i < arr->element_size; i++) {
                ((uint8_t*)cell)[i] = ((uint8_t*)value)[i];
            }
    }
    
    // Mark as present in bitmap
    if (arr->data_presence_map) {
        size_t flat_idx = get_flat_index(arr, x, y, z, t);
        if (!test_bit(arr->data_presence_map, flat_idx)) {
            set_bit(arr->data_presence_map, flat_idx);
            arr->gap_stats.missing_count--;
            
            // Update overall confidence
            arr->gap_stats.overall_confidence = 
                (float)(arr->gap_stats.total_cells - arr->gap_stats.missing_count) / 
                arr->gap_stats.total_cells;
        }
    }
}

// Get value with presence checking
bool array4d_get(Array4D* arr, int x, int y, int z, int t, void* out_value) {
    // Check if data is present
    if (arr->data_presence_map) {
        size_t flat_idx = get_flat_index(arr, x, y, z, t);
        if (!test_bit(arr->data_presence_map, flat_idx)) {
            return false; // No data present
        }
    }
    
    void* cell = get_cell_ptr(arr, x, y, z, t);
    if (!cell) return false; // Out of bounds
    
    // Copy value based on element size
    switch (arr->element_size) {
        case 1: *(uint8_t*)out_value = *(uint8_t*)cell; break;
        case 2: *(uint16_t*)out_value = *(uint16_t*)cell; break;
        case 4: *(uint32_t*)out_value = *(uint32_t*)cell; break;
        case 8: *(uint64_t*)out_value = *(uint64_t*)cell; break;
        default:
            // Generic memcpy for other sizes
            for (size_t i = 0; i < arr->element_size; i++) {
                ((uint8_t*)out_value)[i] = ((uint8_t*)cell)[i];
            }
    }
    
    return true;
}

// Time travel array access
typedef enum {
    TIME_ABSOLUTE,   // Direct time index
    TIME_RELATIVE,   // Relative to current time
    TIME_PAST,       // Steps back from current
    TIME_FUTURE      // Steps forward from current
} TimeDirection;

// Get cell with temporal access
void* array4d_get_temporal(Array4D* arr, int x, int y, int z, int t, TimeDirection dir) {
    int actual_t;
    
    switch (dir) {
        case TIME_PAST:      // <t means "t steps back"
            actual_t = arr->current_time_index - t;
            break;
            
        case TIME_FUTURE:    // >t means "t steps forward"
            actual_t = arr->current_time_index + t;
            break;
            
        case TIME_RELATIVE:  // =t means relative offset
            actual_t = arr->current_time_index + t;
            break;
            
        case TIME_ABSOLUTE:  // Just use t directly
        default:
            actual_t = t;
            break;
    }
    
    return get_cell_ptr(arr, x, y, z, actual_t);
}

// Check if cell has data
bool array4d_has_data(Array4D* arr, int x, int y, int z, int t) {
    if (!arr->data_presence_map) return true; // Assume all present if no map
    
    size_t flat_idx = get_flat_index(arr, x, y, z, t);
    return test_bit(arr->data_presence_map, flat_idx);
}

// Get array statistics
void array4d_get_stats(Array4D* arr, uint32_t* total_cells, uint32_t* missing_cells,
                      float* confidence) {
    if (total_cells) *total_cells = arr->gap_stats.total_cells;
    if (missing_cells) *missing_cells = arr->gap_stats.missing_count;
    if (confidence) *confidence = arr->gap_stats.overall_confidence;
}

// Get memory layout info
const char* array4d_get_layout_name(Array4D* arr) {
    return arr->layout_type == LAYOUT_CONTIGUOUS ? "contiguous" : "temporal_sliced";
}

// Get dimension info
void array4d_get_dimensions(Array4D* arr, uint32_t* dims) {
    for (int i = 0; i < 4; i++) {
        dims[i] = arr->dimensions[i];
    }
}