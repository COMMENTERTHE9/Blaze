// GGGX ALGORITHM - UNIVERSAL COMPUTATIONAL FEASIBILITY PREDICTOR
// Go, Get, Gap, Glimpse, Guess - The oracle for computational tractability
// This header connects both universal GGGX and its solid numbers application

#ifndef GGGX_H
#define GGGX_H

#include <stdint.h>
#include <stdbool.h>

// GGGX (Go, Get, Gap, Guess) Computational Feasibility Prediction
// Four-phase algorithm for predicting computational success before execution

// Complexity classes
typedef enum {
    O_CONSTANT = 1,
    O_LOG_N = 2,
    O_N = 3,
    O_N_LOG_N = 4,
    O_N_SQUARED = 5,
    O_N_CUBED = 6,
    O_EXPONENTIAL = 7,
    O_FACTORIAL = 8
} ComplexityClass;

// Barrier types for computational limits
typedef enum {
    BARRIER_NONE = 0,
    BARRIER_OVERFLOW = 1,
    BARRIER_UNDERFLOW = 2,
    BARRIER_PRECISION = 3,
    BARRIER_MEMORY = 4,
    BARRIER_TIME = 5,
    BARRIER_QUANTUM = 6
} BarrierType;

// GGGX phases
typedef enum {
    GGGX_PHASE_GO = 0,
    GGGX_PHASE_GET = 1,
    GGGX_PHASE_GAP = 2,
    GGGX_PHASE_GLIMPSE = 3,
    GGGX_PHASE_GUESS = 4
} GGGXPhase;

// Computational trace structure
typedef struct {
    uint32_t instruction_count;
    uint32_t memory_usage;
    uint32_t branch_count;
    uint64_t cycles_estimated;
    double energy_estimate;
    uint32_t quantum_ops;
    ComplexityClass complexity_class;
} ComputationalTrace;

// Terminal analysis for barrier detection
typedef struct {
    uint32_t terminal_digits;
    uint64_t gap_magnitude;
    bool has_pattern;
    uint32_t pattern_length;
    double confidence;
} TerminalAnalysis;

// Main GGGX result structure
typedef struct {
    double input_value;
    uint32_t desired_precision;
    uint32_t significant_digits;
    bool has_pattern;
    uint32_t pattern_period;
    char* mathematical_constant;
    
    ComputationalTrace* trace;
    uint32_t algorithm_complexity;
    uint32_t achievable_precision;
    double confidence;
    
    BarrierType barrier;
    TerminalAnalysis* terminal_analysis;
    double zone_score;
    
    char* explanation;
    bool is_feasible;
} GGGXResult;

// GGGX Analysis structure for universal analysis
typedef struct {
    GGGXResult* results[100];  // Store multiple analyses
    uint32_t result_count;
    double average_confidence;
    double success_rate;
} GGGXAnalysis;

// Core GGGX functions
GGGXResult* gggx_go_phase(GGGXResult* result, double value);
GGGXResult* gggx_get_phase(GGGXResult* result);
GGGXResult* gggx_gap_phase(GGGXResult* result);
GGGXResult* gggx_glimpse_phase(GGGXResult* result);
GGGXResult* gggx_guess_phase(GGGXResult* result);

// Complete analysis
GGGXResult* gggx_analyze(double value, uint32_t precision);

// Utility functions
void gggx_free_result(GGGXResult* result);
void gggx_print_result(GGGXResult* result);

// Universal analysis functions
GGGXAnalysis* gggx_universal_analyze(double* values, uint32_t* precisions, uint32_t count);
void gggx_free_analysis(GGGXAnalysis* analysis);

#endif // GGGX_H