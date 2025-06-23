// GGGX ALGORITHM FOR SOLID NUMBERS
// Go, Get, Gap, Glimpse, Guess - The core computational engine

#ifndef GGGX_H
#define GGGX_H

#include "blaze_types.h"
#include "solid_runtime.h"

// GGGX phase identifiers
typedef enum {
    GGGX_PHASE_GO = 0,      // Gather Overall
    GGGX_PHASE_GET = 1,     // Generate Efficient Traces
    GGGX_PHASE_GAP = 2,     // Gauge Actual Precision
    GGGX_PHASE_GLIMPSE = 3, // Glimpse Limiting Mechanisms
    GGGX_PHASE_GUESS = 4    // Guess Effective Solid Specification
} GGGXPhase;

// Computational trace for GET phase
typedef struct {
    uint32_t instruction_count;
    uint32_t memory_accesses;
    uint32_t branch_count;
    uint64_t cycles_estimated;
    double energy_estimate;     // In arbitrary units
    uint32_t quantum_ops;       // Quantum-sensitive operations
} ComputationalTrace;

// Barrier detection result
typedef struct {
    BarrierType detected_barrier;
    uint64_t barrier_magnitude;
    double confidence_score;    // 0.0 to 1.0
    char reasoning[256];        // Human-readable explanation
} BarrierDetection;

// GGGX analysis result
typedef struct {
    // Input parameters
    double input_value;
    uint32_t desired_precision;
    
    // Phase results
    bool phases_completed[5];
    
    // GO phase results
    uint32_t significant_digits;
    bool has_pattern;
    uint32_t pattern_period;
    
    // GET phase results
    ComputationalTrace trace;
    uint32_t algorithm_complexity;  // O(1), O(n), O(n²), etc.
    
    // GAP phase results
    uint32_t achievable_precision;
    uint64_t gap_start_position;
    double precision_confidence;
    
    // GLIMPSE phase results
    BarrierDetection barrier;
    bool has_terminal_pattern;
    uint32_t terminal_length;
    
    // GUESS phase results - final solid number spec
    SolidNumber* result;
    char explanation[512];
} GGGXResult;

// Main GGGX interface
GGGXResult* gggx_analyze(double value, uint32_t desired_precision);
void gggx_free_result(GGGXResult* result);

// Individual phase functions
bool gggx_go_phase(GGGXResult* result, double value);
bool gggx_get_phase(GGGXResult* result);
bool gggx_gap_phase(GGGXResult* result);
bool gggx_glimpse_phase(GGGXResult* result);
bool gggx_guess_phase(GGGXResult* result);

// Utility functions
const char* gggx_barrier_name(BarrierType barrier);
void gggx_print_result(GGGXResult* result);

// Pattern detection
bool gggx_detect_repeating_pattern(const char* digits, uint32_t len, 
                                   uint32_t* period_out, uint32_t* start_out);
bool gggx_detect_mathematical_constant(double value, const char** name_out);

// Computational analysis
uint32_t gggx_estimate_computation_cost(double value, uint32_t precision);
BarrierType gggx_infer_barrier_type(ComputationalTrace* trace, double value);

#endif // GGGX_H