// BLAZE TYPE DEFINITIONS - Shared types for all modules
#ifndef BLAZE_TYPES_H
#define BLAZE_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Time zones for temporal memory
typedef enum {
    ZONE_PAST = 0,
    ZONE_PRESENT = 1,
    ZONE_FUTURE = 2,
    ZONE_UNKNOWN = 3
} TimeZone;

// Symbol types
typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_ARRAY_4D,
    SYMBOL_TEMPORAL,
    SYMBOL_JUMP_LABEL,
    SYMBOL_ERROR_HANDLER
} SymbolType;

// Storage types
typedef enum {
    STORAGE_REGISTER,
    STORAGE_STACK,
    STORAGE_GLOBAL,
    STORAGE_TEMPORAL,
    STORAGE_IMMEDIATE
} StorageType;

// GapMetadata for runtime
typedef struct {
    float confidence_score;
    float migration_threshold;
    TimeZone target_zone;
} GapMetadata;

// Memory prediction
typedef struct {
    uint32_t stack_usage;
    uint32_t temporal_links;
    uint32_t future_zone_usage;
    uint32_t array_usage;
    bool will_overflow;
} MemoryPrediction;

// Execution step
typedef struct {
    uint16_t node_idx;
    bool creates_past_value;
    bool requires_future_value;
    int32_t temporal_order;
    uint16_t dependencies[8];
    uint8_t dep_count;
} ExecutionStep;

#endif // BLAZE_TYPES_H