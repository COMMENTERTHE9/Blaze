// Array4D type definitions
#ifndef ARRAY4D_TYPES_H
#define ARRAY4D_TYPES_H

#include "blaze_types.h"

// Memory layout strategies
typedef enum {
    LAYOUT_CONTIGUOUS,       // Single memory block (small arrays)
    LAYOUT_TEMPORAL_SLICED   // Distributed time slices (large arrays)
} Array4DLayout;

// Enhanced 4D array structure
struct Array4D {
    // Direct memory mapping
    void* base_memory;           // Contiguous block for small arrays
    size_t total_size;           // Total allocated bytes
    
    // Dimensions & strides
    uint32_t dimensions[4];      // [x, y, z, t]
    size_t strides[4];           // Pre-computed byte offsets
    size_t element_size;         // Size of each element
    
    // Memory layout
    Array4DLayout layout_type;
    void** time_slice_ptrs;      // For sliced layout
    TimeZone* slice_zones;       // Zone for each time slice
    
    // GAP tracking (brilliant bitmap approach)
    uint8_t* data_presence_map;  // Bitmap: which cells have data
    float* confidence_map;       // Confidence per cell (optional)
    uint32_t presence_map_size;  // Size in bytes
    
    // Temporal state
    int32_t current_time_index;  // For relative time access
    
    // Missing data tracking
    struct {
        uint32_t missing_count;
        uint32_t total_cells;
        float overall_confidence;
    } gap_stats;
};

#endif // ARRAY4D_TYPES_H