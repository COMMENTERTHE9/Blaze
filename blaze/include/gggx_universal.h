// GGGX UNIVERSAL COMPUTATIONAL FEASIBILITY PREDICTOR
// The computational triage system that predicts tractability BEFORE wasting resources

#ifndef GGGX_UNIVERSAL_H
#define GGGX_UNIVERSAL_H

#include "blaze_types.h"

// Computational zones
typedef enum {
    ZONE_TRACTABLE,     // Zone (0,1) - Feasible with reasonable resources
    ZONE_INTRACTABLE,   // Zone (1,∞) - Exponentially expensive, possibly impossible
    ZONE_BORDERLINE     // Near the boundary, could go either way
} ComputationalZone;

// Problem domains GGGX can analyze
typedef enum {
    DOMAIN_NUMERICAL,      // Number calculations (like solid numbers)
    DOMAIN_AI_TRAINING,    // Machine learning model training
    DOMAIN_OPTIMIZATION,   // Optimization problems
    DOMAIN_SIMULATION,     // Scientific simulations
    DOMAIN_DATABASE,       // Database query optimization
    DOMAIN_ALGORITHM,      // Algorithm selection
    DOMAIN_CRYPTOGRAPHY,   // Cryptographic computations
    DOMAIN_GRAPHICS,       // Rendering and graphics
    DOMAIN_DISTRIBUTED,    // Distributed computing problems
    DOMAIN_QUANTUM         // Quantum computing problems
} ProblemDomain;

// Resource types to consider
typedef struct {
    uint64_t cpu_cycles;           // Estimated CPU cycles needed
    uint64_t memory_bytes;         // Memory requirements
    uint64_t storage_bytes;        // Storage requirements
    double time_seconds;           // Time estimate
    uint32_t parallel_potential;   // How well it parallelizes (0-100%)
    double energy_joules;          // Energy consumption estimate
    uint32_t network_bandwidth;    // Network requirements (bytes/sec)
} ResourceRequirements;

// GO Phase - Search Space Reduction
typedef struct {
    ProblemDomain domain;
    uint64_t search_space_size;     // Size of problem space
    uint32_t decomposition_level;   // How well can we break it down
    char* algorithm_candidates[10]; // Potential algorithms to use
    uint32_t num_candidates;
    double approximation_quality;   // How good are available approximations
} GOPhaseResult;

// GET Phase - Data Collection
typedef struct {
    ResourceRequirements resources;
    uint32_t algorithmic_complexity; // O(1), O(n), O(n²), O(2^n), etc.
    double convergence_rate;         // For iterative methods
    double stability_measure;        // Numerical stability
    bool has_parallel_bottleneck;    // Amdahl's law limitations
    uint32_t data_dependencies;      // How sequential is it
} GETPhaseResult;

// GAP Phase - Confidence Assessment
typedef struct {
    double prediction_confidence;    // 0.0 to 1.0
    char* missing_resources[10];     // What's needed but not available
    uint32_t num_missing;
    double risk_factors[10];         // Potential failure modes
    uint32_t num_risks;
    uint64_t uncertainty_sources;    // Bitmask of uncertainty types
} GAPPhaseResult;

// GUESS Phase - Zone Classification
typedef struct {
    ComputationalZone zone;
    double zone_confidence;          // How sure are we about the zone
    uint64_t tractability_score;     // 0 = impossible, 100 = trivial
    char* recommendation[256];       // What to do about it
    bool can_approximate;            // Can we make it tractable with approximation
    double approximation_error;      // If we approximate, how much error
} GUESSPhaseResult;

// Complete GGGX Analysis Result
typedef struct {
    // Input problem description
    char problem_description[512];
    ProblemDomain domain;
    
    // Phase results
    GOPhaseResult go;
    GETPhaseResult get;
    GAPPhaseResult gap;
    GUESSPhaseResult guess;
    
    // Overall verdict
    bool should_attempt;             // The big decision
    char rationale[1024];            // Why or why not
    
    // Alternative approaches if intractable
    char* alternatives[5];
    uint32_t num_alternatives;
} GGGXAnalysis;

// Universal GGGX interface
GGGXAnalysis* gggx_analyze_problem(const char* problem_description, 
                                   ProblemDomain domain,
                                   void* domain_specific_data);

// Domain-specific analyzers
GGGXAnalysis* gggx_analyze_ai_training(void* model_params);
GGGXAnalysis* gggx_analyze_optimization(void* optimization_problem);
GGGXAnalysis* gggx_analyze_simulation(void* simulation_params);
GGGXAnalysis* gggx_analyze_database_query(void* query_plan);
GGGXAnalysis* gggx_analyze_algorithm(void* algorithm_spec);

// Utility functions
const char* gggx_zone_name(ComputationalZone zone);
const char* gggx_domain_name(ProblemDomain domain);
void gggx_print_analysis(GGGXAnalysis* analysis);
void gggx_free_analysis(GGGXAnalysis* analysis);

// Zone prediction functions
ComputationalZone gggx_predict_zone(uint64_t complexity, 
                                   ResourceRequirements* resources);
double gggx_zone_boundary_distance(GGGXAnalysis* analysis);

// Resource scaling predictions
ResourceRequirements* gggx_scale_resources(ResourceRequirements* base,
                                          uint64_t input_size_factor);

#endif // GGGX_UNIVERSAL_H