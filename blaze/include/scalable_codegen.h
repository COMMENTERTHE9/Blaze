// SCALABLE CODE GENERATION FOR INDUSTRIAL COMPUTATIONS
// Supports multi-GB code generation through segmented buffers and streaming

#ifndef SCALABLE_CODEGEN_H
#define SCALABLE_CODEGEN_H

#include "blaze_types.h"

// Segment size - 16MB per segment for better cache usage
#define SEGMENT_SIZE (16 * 1024 * 1024)
#define MAX_SEGMENTS 256  // Up to 4GB total

// Streaming modes
typedef enum {
    STREAM_NONE = 0,      // Keep everything in memory
    STREAM_THRESHOLD,     // Stream when exceeding threshold
    STREAM_ALWAYS        // Always stream to disk
} StreamingMode;

// Code segment - individual buffer
typedef struct CodeSegment {
    uint8_t* code;
    uint32_t size;
    uint32_t position;
    struct CodeSegment* next;
} CodeSegment;

// Scalable code generation context
typedef struct {
    // Primary buffer (always in memory)
    CodeBuffer primary;
    
    // Overflow segments (linked list)
    CodeSegment* segments;
    CodeSegment* current_segment;
    uint32_t segment_count;
    
    // Total code generated
    uint64_t total_size;
    
    // Streaming configuration
    StreamingMode stream_mode;
    uint64_t stream_threshold;  // Bytes before streaming
    int output_fd;              // File descriptor for streaming
    char* output_path;          // Path for memory-mapped file
    
    // Memory mapping support
    bool use_mmap;
    void* mmap_base;
    uint64_t mmap_size;
    
    // Error tracking
    bool has_error;
    const char* error_msg;
    
    // Performance metrics
    uint64_t segments_allocated;
    uint64_t bytes_streamed;
    uint64_t peak_memory;
} ScalableCodeGen;

// Label fixup for cross-segment jumps
typedef struct LabelFixup {
    uint64_t offset;        // Absolute offset in generated code
    uint32_t label_id;      // Label being referenced
    uint8_t size;           // Size of fixup (1, 2, 4, or 8 bytes)
    struct LabelFixup* next;
} LabelFixup;

// Label definition
typedef struct Label {
    uint32_t id;
    uint64_t offset;        // Absolute offset where label is defined
    bool defined;
    struct Label* next;
} Label;

// Extended scalable context with labels
typedef struct {
    ScalableCodeGen gen;
    
    // Label management
    Label* labels;
    LabelFixup* fixups;
    uint32_t next_label_id;
} ScalableContext;

// Initialize scalable code generation
void scalable_init(ScalableContext* ctx, uint32_t initial_size, StreamingMode mode);

// Set up file streaming
bool scalable_setup_streaming(ScalableContext* ctx, const char* output_path);

// Set up memory mapping
bool scalable_setup_mmap(ScalableContext* ctx, const char* output_path, uint64_t size);

// Emit single byte
void scalable_emit_byte(ScalableContext* ctx, uint8_t byte);

// Emit multiple bytes
void scalable_emit_bytes(ScalableContext* ctx, const uint8_t* bytes, uint32_t count);

// Emit word (16-bit)
void scalable_emit_word(ScalableContext* ctx, uint16_t word);

// Emit dword (32-bit)
void scalable_emit_dword(ScalableContext* ctx, uint32_t dword);

// Emit qword (64-bit)
void scalable_emit_qword(ScalableContext* ctx, uint64_t qword);

// Get current position (for jump calculations)
uint64_t scalable_get_position(ScalableContext* ctx);

// Create a new label
uint32_t scalable_create_label(ScalableContext* ctx);

// Define label at current position
void scalable_define_label(ScalableContext* ctx, uint32_t label_id);

// Emit jump to label (with fixup if needed)
void scalable_emit_jump_label(ScalableContext* ctx, uint32_t label_id, bool is_near);

// Allocate new segment
bool scalable_allocate_segment(ScalableContext* ctx);

// Finalize and write all segments
bool scalable_finalize(ScalableContext* ctx);

// Clean up resources
void scalable_cleanup(ScalableContext* ctx);

// Performance statistics
void scalable_print_stats(ScalableContext* ctx);

// Compatibility layer - wrap existing CodeBuffer functions
void scalable_wrap_buffer(ScalableContext* ctx, CodeBuffer* buf);

// Direct buffer access for compatibility
CodeBuffer* scalable_get_active_buffer(ScalableContext* ctx);

#endif // SCALABLE_CODEGEN_H