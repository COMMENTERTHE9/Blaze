#ifndef GGGX_INTEGRATION_H
#define GGGX_INTEGRATION_H

#include "gggx.h"
#include "gggx_universal.h"
#include "blaze_internals.h"

// GGGX Integration - Exposing Every Computational Step to the User

// Initialize GGGX execution engine
void gggx_init_engine(void);

// GGGX Phase Functions - Exposed to User
GGGXResult* gggx_go_phase_execute(double value, uint32_t precision);
bool gggx_get_phase_execute(GGGXResult* result);
bool gggx_gap_phase_execute(GGGXResult* result);
bool gggx_glimpse_phase_execute(GGGXResult* result);
bool gggx_guess_phase_execute(GGGXResult* result);

// Fine-Grained Sub-Step Functions

// GO Phase Sub-Steps
uint32_t gggx_analyze_significant_digits(double value);
bool gggx_detect_patterns(double value, uint32_t* period_out);
bool gggx_check_mathematical_constants(double value, char** constant_name);

// GET Phase Sub-Steps
ComputationalTrace* gggx_generate_computational_trace(double value, uint32_t precision);
uint32_t gggx_estimate_algorithm_complexity(double value, uint32_t precision);

// GAP Phase Sub-Steps
uint32_t gggx_assess_achievable_precision(ComputationalTrace* trace, uint32_t desired_precision);
double gggx_calculate_confidence(uint32_t achievable_precision, uint32_t desired_precision, uint32_t complexity);

// GLIMPSE Phase Sub-Steps
BarrierType gggx_detect_barriers(ComputationalTrace* trace, double value);
TerminalAnalysis* gggx_analyze_terminal_digits(double value, BarrierType barrier, uint64_t gap_magnitude);

// GUESS Phase Sub-Steps
double gggx_classify_zone(uint32_t complexity, double confidence, BarrierType barrier);

// User Override Functions
void gggx_set_user_go_phase(void (*user_func)(GGGXResult*, double));
void gggx_set_user_get_phase(void (*user_func)(GGGXResult*));
void gggx_set_user_gap_phase(void (*user_func)(GGGXResult*));
void gggx_set_user_glimpse_phase(void (*user_func)(GGGXResult*));
void gggx_set_user_guess_phase(void (*user_func)(GGGXResult*));

// Fine-Grained Control
void gggx_enable_phase(GGGXPhase phase, bool enable);

// Status and Debugging
bool gggx_is_phase_completed(GGGXPhase phase);
bool gggx_is_sub_step_completed(const char* sub_step_name);
void gggx_print_status(void);

// Complete GGGX Analysis with User Control
GGGXResult* gggx_analyze_with_control(double value, uint32_t precision);

// Blaze Syntax Integration Functions
// These functions will be called from the Blaze parser/interpreter

// Parse GGGX phase invocation: gggx.go/ value /
GGGXResult* blaze_parse_gggx_go_phase(ASTNode* node);

// Parse GGGX sub-step invocation: gggx.go.analyze_digits/ value /
uint32_t blaze_parse_gggx_sub_step(ASTNode* node);

// Parse GGGX user override: gggx.go = my_custom_go_phase/
void blaze_parse_gggx_user_override(ASTNode* node);

// Parse GGGX phase enable/disable: gggx.enable.go/ true /
void blaze_parse_gggx_phase_control(ASTNode* node);

// Parse GGGX status check: gggx.status.go/
bool blaze_parse_gggx_status_check(ASTNode* node);

#endif // GGGX_INTEGRATION_H 