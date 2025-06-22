// 4D ARRAY GAP ANALYSIS
// Missing data detection and prediction for temporal arrays

#include "blaze_internals.h"
#include "array4d_types.h"

// Forward declaration from array4d.c
// Array4D is defined in blaze_internals.h
extern bool array4d_has_data(Array4D* arr, int x, int y, int z, int t);
extern void* get_cell_ptr(Array4D* arr, int x, int y, int z, int t);
extern size_t get_flat_index(Array4D* arr, int x, int y, int z, int t);
extern void set_bit(uint8_t* map, size_t bit_idx);

// Missing data pattern types
typedef enum {
    MISSING_RANDOM,           // Random missing cells
    MISSING_TEMPORAL_GAP,     // Missing time slices
    MISSING_SPATIAL_REGION,   // Missing spatial regions
    MISSING_SYSTEMATIC        // Systematic pattern
} MissingPattern;

// GAP analysis result
typedef struct {
    MissingPattern pattern_type;
    float completeness;           // 0.0 to 1.0
    float confidence;            // Overall confidence
    
    struct {
        uint32_t missing_time_slices[16];
        uint32_t num_missing_slices;
    } temporal_gaps;
    
    struct {
        uint32_t x_start, x_end;
        uint32_t y_start, y_end;
        uint32_t z_start, z_end;
    } spatial_gaps[8];
    uint32_t num_spatial_gaps;
} GapAnalysisResult;

// Analyze missing data patterns in 4D array
GapAnalysisResult array4d_analyze_gaps(Array4D* arr) {
    GapAnalysisResult result = {0};
    
    if (!arr->data_presence_map) {
        result.completeness = 1.0;
        result.confidence = 1.0;
        return result;
    }
    
    // Calculate completeness
    uint32_t present_count = arr->gap_stats.total_cells - arr->gap_stats.missing_count;
    result.completeness = (float)present_count / arr->gap_stats.total_cells;
    
    // Analyze temporal gaps - check each time slice
    for (uint32_t t = 0; t < arr->dimensions[3]; t++) {
        bool slice_has_data = false;
        
        for (uint32_t z = 0; z < arr->dimensions[2] && !slice_has_data; z++) {
            for (uint32_t y = 0; y < arr->dimensions[1] && !slice_has_data; y++) {
                for (uint32_t x = 0; x < arr->dimensions[0]; x++) {
                    if (array4d_has_data(arr, x, y, z, t)) {
                        slice_has_data = true;
                        break;
                    }
                }
            }
        }
        
        if (!slice_has_data && result.temporal_gaps.num_missing_slices < 16) {
            result.temporal_gaps.missing_time_slices[result.temporal_gaps.num_missing_slices++] = t;
        }
    }
    
    // Determine pattern type
    if (result.temporal_gaps.num_missing_slices > arr->dimensions[3] / 4) {
        result.pattern_type = MISSING_TEMPORAL_GAP;
    } else if (result.completeness < 0.3) {
        result.pattern_type = MISSING_SYSTEMATIC;
    } else {
        result.pattern_type = MISSING_RANDOM;
    }
    
    // Calculate confidence based on pattern and completeness
    if (result.pattern_type == MISSING_TEMPORAL_GAP) {
        // Can interpolate between time slices
        result.confidence = result.completeness * 0.8;
    } else if (result.pattern_type == MISSING_RANDOM) {
        // Random missing is harder to predict
        result.confidence = result.completeness * 0.6;
    } else {
        result.confidence = result.completeness * 0.5;
    }
    
    return result;
}

// Helper: Get neighboring values for interpolation
typedef struct {
    double values[6];  // Up to 6 neighbors (x±1, y±1, z±1)
    uint8_t count;
} NeighborData;

static NeighborData get_spatial_neighbors(Array4D* arr, int x, int y, int z, int t) {
    NeighborData neighbors = {0};
    
    // Check all 6 spatial neighbors
    int offsets[6][3] = {
        {-1, 0, 0}, {1, 0, 0},  // x neighbors
        {0, -1, 0}, {0, 1, 0},  // y neighbors
        {0, 0, -1}, {0, 0, 1}   // z neighbors
    };
    
    for (int i = 0; i < 6; i++) {
        int nx = x + offsets[i][0];
        int ny = y + offsets[i][1];
        int nz = z + offsets[i][2];
        
        if (nx >= 0 && nx < arr->dimensions[0] &&
            ny >= 0 && ny < arr->dimensions[1] &&
            nz >= 0 && nz < arr->dimensions[2]) {
            
            if (array4d_has_data(arr, nx, ny, nz, t)) {
                void* cell = get_cell_ptr(arr, nx, ny, nz, t);
                if (cell && arr->element_size == sizeof(double)) {
                    neighbors.values[neighbors.count++] = *(double*)cell;
                }
            }
        }
    }
    
    return neighbors;
}

// Interpolate missing value from neighbors
double array4d_interpolate_missing(Array4D* arr, int x, int y, int z, int t) {
    // First try spatial interpolation
    NeighborData spatial = get_spatial_neighbors(arr, x, y, z, t);
    
    if (spatial.count >= 2) {
        // Average of spatial neighbors
        double sum = 0.0;
        for (int i = 0; i < spatial.count; i++) {
            sum += spatial.values[i];
        }
        return sum / spatial.count;
    }
    
    // Try temporal interpolation
    double before = 0.0, after = 0.0;
    bool has_before = false, has_after = false;
    
    // Look for previous time value
    for (int dt = 1; dt <= 3 && t - dt >= 0; dt++) {
        if (array4d_has_data(arr, x, y, z, t - dt)) {
            void* cell = get_cell_ptr(arr, x, y, z, t - dt);
            if (cell && arr->element_size == sizeof(double)) {
                before = *(double*)cell;
                has_before = true;
                break;
            }
        }
    }
    
    // Look for future time value
    for (int dt = 1; dt <= 3 && t + dt < arr->dimensions[3]; dt++) {
        if (array4d_has_data(arr, x, y, z, t + dt)) {
            void* cell = get_cell_ptr(arr, x, y, z, t + dt);
            if (cell && arr->element_size == sizeof(double)) {
                after = *(double*)cell;
                has_after = true;
                break;
            }
        }
    }
    
    if (has_before && has_after) {
        // Linear interpolation between time points
        return (before + after) / 2.0;
    } else if (has_before) {
        return before;
    } else if (has_after) {
        return after;
    }
    
    // No neighbors found - return default
    return 0.0;
}

// Fill missing values using interpolation
uint32_t array4d_fill_missing(Array4D* arr, float confidence_threshold) {
    if (!arr->data_presence_map || arr->element_size != sizeof(double)) {
        return 0; // Can only interpolate doubles for now
    }
    
    uint32_t filled_count = 0;
    GapAnalysisResult gap_result = array4d_analyze_gaps(arr);
    
    // Only fill if confidence is above threshold
    if (gap_result.confidence < confidence_threshold) {
        return 0;
    }
    
    // Iterate through all cells
    for (uint32_t t = 0; t < arr->dimensions[3]; t++) {
        for (uint32_t z = 0; z < arr->dimensions[2]; z++) {
            for (uint32_t y = 0; y < arr->dimensions[1]; y++) {
                for (uint32_t x = 0; x < arr->dimensions[0]; x++) {
                    if (!array4d_has_data(arr, x, y, z, t)) {
                        // Try to interpolate
                        double predicted = array4d_interpolate_missing(arr, x, y, z, t);
                        
                        // Only fill if we got a valid prediction
                        if (predicted != 0.0) { // Simple validation
                            array4d_set(arr, x, y, z, t, &predicted);
                            filled_count++;
                        }
                    }
                }
            }
        }
    }
    
    return filled_count;
}

// Create confidence map for array
void array4d_create_confidence_map(Array4D* arr) {
    if (arr->confidence_map) return; // Already exists
    
    size_t map_size = arr->gap_stats.total_cells * sizeof(float);
    arr->confidence_map = (float*)temporal_alloc_var("confidence_map", map_size, ZONE_PRESENT);
    
    if (!arr->confidence_map) return;
    
    // Initialize confidence based on data presence and neighbors
    for (uint32_t i = 0; i < arr->gap_stats.total_cells; i++) {
        if (test_bit(arr->data_presence_map, i)) {
            // Present data has full confidence
            arr->confidence_map[i] = 1.0;
        } else {
            // Missing data confidence based on neighbor availability
            // Convert flat index back to coordinates
            uint32_t temp = i;
            uint32_t x = temp % arr->dimensions[0];
            temp /= arr->dimensions[0];
            uint32_t y = temp % arr->dimensions[1];
            temp /= arr->dimensions[1];
            uint32_t z = temp % arr->dimensions[2];
            uint32_t t = temp / arr->dimensions[2];
            
            NeighborData neighbors = get_spatial_neighbors(arr, x, y, z, t);
            
            // Confidence based on neighbor count
            if (neighbors.count >= 4) {
                arr->confidence_map[i] = 0.9;
            } else if (neighbors.count >= 2) {
                arr->confidence_map[i] = 0.7;
            } else if (neighbors.count == 1) {
                arr->confidence_map[i] = 0.5;
            } else {
                arr->confidence_map[i] = 0.1;
            }
        }
    }
}

// Get cell confidence
float array4d_get_confidence(Array4D* arr, int x, int y, int z, int t) {
    if (!arr->confidence_map) {
        // No confidence map - return based on presence
        return array4d_has_data(arr, x, y, z, t) ? 1.0 : 0.0;
    }
    
    size_t flat_idx = get_flat_index(arr, x, y, z, t);
    if (flat_idx < arr->gap_stats.total_cells) {
        return arr->confidence_map[flat_idx];
    }
    
    return 0.0;
}

// Identify missing data reason (for debugging/analysis)
const char* array4d_missing_reason(Array4D* arr, int x, int y, int z, int t) {
    if (array4d_has_data(arr, x, y, z, t)) {
        return "data_present";
    }
    
    // Check if entire time slice is missing
    bool slice_empty = true;
    for (uint32_t i = 0; i < arr->dimensions[0] * arr->dimensions[1] * arr->dimensions[2]; i++) {
        uint32_t check_x = i % arr->dimensions[0];
        uint32_t check_y = (i / arr->dimensions[0]) % arr->dimensions[1];
        uint32_t check_z = i / (arr->dimensions[0] * arr->dimensions[1]);
        
        if (array4d_has_data(arr, check_x, check_y, check_z, t)) {
            slice_empty = false;
            break;
        }
    }
    
    if (slice_empty) {
        return "temporal_gap";
    }
    
    // Check for spatial patterns
    NeighborData neighbors = get_spatial_neighbors(arr, x, y, z, t);
    if (neighbors.count == 0) {
        return "isolated_missing";
    } else if (neighbors.count < 3) {
        return "sparse_region";
    }
    
    return "random_missing";
}