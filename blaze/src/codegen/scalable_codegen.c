// SCALABLE CODE GENERATION IMPLEMENTATION
// Handles multi-GB code generation for industrial computations

#include "blaze_internals.h"
#include "scalable_codegen.h"

// File I/O functions are declared in blaze_internals.h and implemented in elf_generator.c

// Initialize scalable code generation
void scalable_init(ScalableContext* ctx, uint32_t initial_size, StreamingMode mode) {
    // Zero out the structure
    for (int i = 0; i < sizeof(ScalableContext); i++) {
        ((uint8_t*)ctx)[i] = 0;
    }
    
    // Initialize primary buffer
    if (initial_size == 0) {
        initial_size = 1024 * 1024; // Default 1MB
    }
    
    // Allocate primary buffer
    ctx->gen.primary.code = (uint8_t*)syscall6(SYS_MMAP, 0, initial_size,
                                               0x3, // PROT_READ | PROT_WRITE
                                               0x22, // MAP_PRIVATE | MAP_ANONYMOUS
                                               -1, 0);
    
    if ((long)ctx->gen.primary.code == -1) {
        ctx->gen.has_error = true;
        ctx->gen.error_msg = "Failed to allocate primary buffer";
        return;
    }
    
    ctx->gen.primary.capacity = initial_size;
    ctx->gen.primary.position = 0;
    ctx->gen.primary.has_error = false;
    
    // Set streaming mode
    ctx->gen.stream_mode = mode;
    ctx->gen.stream_threshold = 64 * 1024 * 1024; // 64MB default threshold
    
    // Initialize metrics
    ctx->gen.total_size = 0;
    ctx->gen.segments_allocated = 0;
    ctx->gen.bytes_streamed = 0;
    ctx->gen.peak_memory = initial_size;
    
    print_str("[SCALABLE] Initialized with ");
    print_num(initial_size);
    print_str(" byte primary buffer\n");
}

// Allocate a new segment
bool scalable_allocate_segment(ScalableContext* ctx) {
    print_str("[SCALABLE] Allocating new segment\n");
    
    // Check segment limit
    if (ctx->gen.segment_count >= MAX_SEGMENTS) {
        ctx->gen.has_error = true;
        ctx->gen.error_msg = "Maximum segment count reached";
        return false;
    }
    
    // Allocate segment structure
    CodeSegment* seg = (CodeSegment*)syscall6(SYS_MMAP, 0, sizeof(CodeSegment),
                                              0x3, 0x22, -1, 0);
    if ((long)seg == -1) {
        ctx->gen.has_error = true;
        ctx->gen.error_msg = "Failed to allocate segment structure";
        return false;
    }
    
    // Allocate segment buffer
    seg->code = (uint8_t*)syscall6(SYS_MMAP, 0, SEGMENT_SIZE,
                                   0x3, 0x22, -1, 0);
    if ((long)seg->code == -1) {
        syscall6(SYS_MUNMAP, (long)seg, sizeof(CodeSegment), 0, 0, 0, 0);
        ctx->gen.has_error = true;
        ctx->gen.error_msg = "Failed to allocate segment buffer";
        return false;
    }
    
    seg->size = SEGMENT_SIZE;
    seg->position = 0;
    seg->next = NULL;
    
    // Link segment
    if (ctx->gen.segments == NULL) {
        ctx->gen.segments = seg;
        ctx->gen.current_segment = seg;
    } else {
        ctx->gen.current_segment->next = seg;
        ctx->gen.current_segment = seg;
    }
    
    ctx->gen.segment_count++;
    ctx->gen.segments_allocated++;
    
    // Update peak memory
    uint64_t current_memory = ctx->gen.primary.capacity + 
                             (ctx->gen.segment_count * SEGMENT_SIZE);
    if (current_memory > ctx->gen.peak_memory) {
        ctx->gen.peak_memory = current_memory;
    }
    
    print_str("[SCALABLE] Allocated segment ");
    print_num(ctx->gen.segment_count);
    print_str(" (");
    print_num(SEGMENT_SIZE);
    print_str(" bytes)\n");
    
    return true;
}

// Get current active buffer and position
static void get_active_buffer(ScalableContext* ctx, uint8_t** buffer, uint32_t* position, uint32_t* remaining) {
    if (ctx->gen.total_size < ctx->gen.primary.capacity) {
        // Still in primary buffer
        *buffer = ctx->gen.primary.code;
        *position = ctx->gen.primary.position;
        *remaining = ctx->gen.primary.capacity - ctx->gen.primary.position;
    } else if (ctx->gen.current_segment != NULL) {
        // In overflow segment
        *buffer = ctx->gen.current_segment->code;
        *position = ctx->gen.current_segment->position;
        *remaining = ctx->gen.current_segment->size - ctx->gen.current_segment->position;
    } else {
        // Need to allocate first segment
        *buffer = NULL;
        *position = 0;
        *remaining = 0;
    }
}

// Update position after writing
static void update_position(ScalableContext* ctx, uint32_t bytes_written) {
    ctx->gen.total_size += bytes_written;
    
    if (ctx->gen.total_size <= ctx->gen.primary.capacity) {
        ctx->gen.primary.position += bytes_written;
    } else if (ctx->gen.current_segment != NULL) {
        ctx->gen.current_segment->position += bytes_written;
    }
}

// Emit single byte
void scalable_emit_byte(ScalableContext* ctx, uint8_t byte) {
    if (ctx->gen.has_error) return;
    
    uint8_t* buffer;
    uint32_t position, remaining;
    get_active_buffer(ctx, &buffer, &position, &remaining);
    
    // Check if we need a new segment
    if (remaining == 0) {
        if (!scalable_allocate_segment(ctx)) {
            return;
        }
        get_active_buffer(ctx, &buffer, &position, &remaining);
    }
    
    // Write byte
    buffer[position] = byte;
    update_position(ctx, 1);
    
    // Check streaming threshold
    if (ctx->gen.stream_mode == STREAM_THRESHOLD && 
        ctx->gen.total_size > ctx->gen.stream_threshold &&
        ctx->gen.output_fd > 0) {
        // TODO: Implement streaming flush
    }
}

// Emit multiple bytes
void scalable_emit_bytes(ScalableContext* ctx, const uint8_t* bytes, uint32_t count) {
    if (ctx->gen.has_error) return;
    
    uint32_t written = 0;
    while (written < count) {
        uint8_t* buffer;
        uint32_t position, remaining;
        get_active_buffer(ctx, &buffer, &position, &remaining);
        
        if (remaining == 0) {
            if (!scalable_allocate_segment(ctx)) {
                return;
            }
            get_active_buffer(ctx, &buffer, &position, &remaining);
        }
        
        // Copy what we can
        uint32_t to_copy = count - written;
        if (to_copy > remaining) {
            to_copy = remaining;
        }
        
        // Manual copy (no memcpy in nostdlib)
        for (uint32_t i = 0; i < to_copy; i++) {
            buffer[position + i] = bytes[written + i];
        }
        
        written += to_copy;
        update_position(ctx, to_copy);
    }
}

// Emit word (16-bit)
void scalable_emit_word(ScalableContext* ctx, uint16_t word) {
    scalable_emit_byte(ctx, word & 0xFF);
    scalable_emit_byte(ctx, (word >> 8) & 0xFF);
}

// Emit dword (32-bit)
void scalable_emit_dword(ScalableContext* ctx, uint32_t dword) {
    scalable_emit_byte(ctx, dword & 0xFF);
    scalable_emit_byte(ctx, (dword >> 8) & 0xFF);
    scalable_emit_byte(ctx, (dword >> 16) & 0xFF);
    scalable_emit_byte(ctx, (dword >> 24) & 0xFF);
}

// Emit qword (64-bit)
void scalable_emit_qword(ScalableContext* ctx, uint64_t qword) {
    scalable_emit_dword(ctx, qword & 0xFFFFFFFF);
    scalable_emit_dword(ctx, (qword >> 32) & 0xFFFFFFFF);
}

// Get current position
uint64_t scalable_get_position(ScalableContext* ctx) {
    return ctx->gen.total_size;
}

// Set up file streaming
bool scalable_setup_streaming(ScalableContext* ctx, const char* output_path) {
    if (ctx->gen.has_error) return false;
    
    // Open output file
    ctx->gen.output_fd = syscall_open(output_path, 
                                     O_WRONLY | O_CREAT | O_TRUNC, 
                                     0755);
    
    if (ctx->gen.output_fd < 0) {
        ctx->gen.has_error = true;
        ctx->gen.error_msg = "Failed to open output file for streaming";
        return false;
    }
    
    // Store path for potential mmap later
    uint32_t len = str_len(output_path);
    ctx->gen.output_path = (char*)syscall6(SYS_MMAP, 0, len + 1,
                                          0x3, 0x22, -1, 0);
    if ((long)ctx->gen.output_path != -1) {
        for (uint32_t i = 0; i <= len; i++) {
            ctx->gen.output_path[i] = output_path[i];
        }
    }
    
    print_str("[SCALABLE] Streaming enabled to: ");
    print_str(output_path);
    print_str("\n");
    
    return true;
}

// Finalize and write all segments
bool scalable_finalize(ScalableContext* ctx) {
    if (ctx->gen.has_error) return false;
    
    print_str("[SCALABLE] Finalizing code generation\n");
    print_str("  Total size: ");
    print_num(ctx->gen.total_size);
    print_str(" bytes\n");
    
    if (ctx->gen.output_fd <= 0) {
        // No output file, everything stays in memory
        return true;
    }
    
    // Write primary buffer
    if (ctx->gen.primary.position > 0) {
        int64_t written = syscall_write(ctx->gen.output_fd, 
                                       ctx->gen.primary.code,
                                       ctx->gen.primary.position);
        if (written != ctx->gen.primary.position) {
            ctx->gen.has_error = true;
            ctx->gen.error_msg = "Failed to write primary buffer";
            return false;
        }
        ctx->gen.bytes_streamed += written;
    }
    
    // Write all segments
    CodeSegment* seg = ctx->gen.segments;
    while (seg != NULL) {
        if (seg->position > 0) {
            int64_t written = syscall_write(ctx->gen.output_fd,
                                          seg->code,
                                          seg->position);
            if (written != seg->position) {
                ctx->gen.has_error = true;
                ctx->gen.error_msg = "Failed to write segment";
                return false;
            }
            ctx->gen.bytes_streamed += written;
        }
        seg = seg->next;
    }
    
    print_str("  Bytes streamed: ");
    print_num(ctx->gen.bytes_streamed);
    print_str("\n");
    
    return true;
}

// Clean up resources
void scalable_cleanup(ScalableContext* ctx) {
    print_str("[SCALABLE] Cleaning up resources\n");
    
    // Close output file
    if (ctx->gen.output_fd > 0) {
        syscall_close(ctx->gen.output_fd);
        ctx->gen.output_fd = 0;
    }
    
    // Free output path
    if (ctx->gen.output_path != NULL) {
        uint32_t len = str_len(ctx->gen.output_path);
        syscall6(SYS_MUNMAP, (long)ctx->gen.output_path, len + 1, 0, 0, 0, 0);
        ctx->gen.output_path = NULL;
    }
    
    // Free primary buffer
    if (ctx->gen.primary.code != NULL) {
        syscall6(SYS_MUNMAP, (long)ctx->gen.primary.code, 
                ctx->gen.primary.capacity, 0, 0, 0, 0);
        ctx->gen.primary.code = NULL;
    }
    
    // Free all segments
    CodeSegment* seg = ctx->gen.segments;
    while (seg != NULL) {
        CodeSegment* next = seg->next;
        if (seg->code != NULL) {
            syscall6(SYS_MUNMAP, (long)seg->code, seg->size, 0, 0, 0, 0);
        }
        syscall6(SYS_MUNMAP, (long)seg, sizeof(CodeSegment), 0, 0, 0, 0);
        seg = next;
    }
    
    ctx->gen.segments = NULL;
    ctx->gen.current_segment = NULL;
    ctx->gen.segment_count = 0;
}

// Print performance statistics
void scalable_print_stats(ScalableContext* ctx) {
    print_str("\n[SCALABLE] Performance Statistics:\n");
    print_str("  Total code generated: ");
    print_num(ctx->gen.total_size);
    print_str(" bytes\n");
    print_str("  Segments allocated: ");
    print_num(ctx->gen.segments_allocated);
    print_str("\n");
    print_str("  Peak memory usage: ");
    print_num(ctx->gen.peak_memory);
    print_str(" bytes\n");
    print_str("  Bytes streamed to disk: ");
    print_num(ctx->gen.bytes_streamed);
    print_str("\n");
    
    if (ctx->gen.total_size > 0) {
        uint64_t efficiency = (ctx->gen.bytes_streamed * 100) / ctx->gen.total_size;
        print_str("  Streaming efficiency: ");
        print_num(efficiency);
        print_str("%\n");
    }
}

// Compatibility wrapper for existing CodeBuffer
void scalable_wrap_buffer(ScalableContext* ctx, CodeBuffer* buf) {
    // This allows gradual migration from CodeBuffer to ScalableContext
    ctx->gen.primary = *buf;
}

// Get active buffer for compatibility
CodeBuffer* scalable_get_active_buffer(ScalableContext* ctx) {
    // For compatibility with existing code
    // Note: This only returns primary buffer
    return &ctx->gen.primary;
}