#include <sentry.h>
#include <stdio.h>
#include <string.h>
#include "sentry_integration.h"

// Initialize Sentry for Blaze compiler
void init_sentry() {
    sentry_options_t *options = sentry_options_new();
    
    // Sentry DSN for Blaze Compiler
    sentry_options_set_dsn(options, "https://903718515ee95abc1f9b4b5c4752461b@o4509528354390016.ingest.us.sentry.io/4509528390369280");
    
    // Set release version
    sentry_options_set_release(options, "blaze-compiler@0.1.0");
    
    // Set environment
    sentry_options_set_environment(options, "development");
    
    // Set database path for caching
    sentry_options_set_database_path(options, ".sentry-blaze");
    
    // Enable debug mode during development
    #ifdef DEBUG
    sentry_options_set_debug(options, 1);
    #endif
    
    // Initialize Sentry
    sentry_init(options);
    
    // Log initialization
    sentry_capture_event(sentry_value_new_message_event(
        SENTRY_LEVEL_INFO,
        "blaze",
        "Blaze compiler initialized with Sentry"
    ));
}

// Report a compiler error to Sentry
void report_compiler_error(const char *error_type, const char *message, const char *file, int line) {
    sentry_value_t event = sentry_value_new_event();
    
    // Set error message
    char full_message[1024];
    snprintf(full_message, sizeof(full_message), "%s: %s (at %s:%d)", error_type, message, file, line);
    sentry_value_set_by_key(event, "message", sentry_value_new_string(full_message));
    sentry_value_set_by_key(event, "level", sentry_value_new_string("error"));
    
    // Add exception info
    sentry_value_t exc = sentry_value_new_exception(error_type, message);
    sentry_value_t exc_value = sentry_value_new_object();
    sentry_value_set_by_key(exc_value, "filename", sentry_value_new_string(file));
    sentry_value_set_by_key(exc_value, "lineno", sentry_value_new_int32(line));
    sentry_value_set_by_key(exc, "value", exc_value);
    sentry_event_add_exception(event, exc);
    
    // Add context
    sentry_value_t compiler_context = sentry_value_new_object();
    sentry_value_set_by_key(compiler_context, "error_type", sentry_value_new_string(error_type));
    sentry_value_set_by_key(compiler_context, "source_file", sentry_value_new_string(file));
    sentry_value_set_by_key(compiler_context, "source_line", sentry_value_new_int32(line));
    
    sentry_value_t contexts = sentry_value_new_object();
    sentry_value_set_by_key(contexts, "compiler", compiler_context);
    sentry_value_set_by_key(event, "contexts", contexts);
    
    sentry_capture_event(event);
}

// Add breadcrumb for tracking compilation steps
void add_compilation_breadcrumb(const char *step, const char *details) {
    sentry_value_t crumb = sentry_value_new_breadcrumb("default", step);
    sentry_value_set_by_key(crumb, "category", sentry_value_new_string("compilation"));
    sentry_value_set_by_key(crumb, "level", sentry_value_new_string("info"));
    
    if (details) {
        sentry_value_t data = sentry_value_new_object();
        sentry_value_set_by_key(data, "details", sentry_value_new_string(details));
        sentry_value_set_by_key(crumb, "data", data);
    }
    
    sentry_add_breadcrumb(crumb);
}

// Report AST node errors
void report_ast_error(const char *node_type, int expected_type, int actual_type, const char *context) {
    char message[512];
    snprintf(message, sizeof(message), 
             "AST node type mismatch: expected %d, got %d for %s", 
             expected_type, actual_type, node_type);
    
    sentry_value_t event = sentry_value_new_event();
    sentry_value_set_by_key(event, "message", sentry_value_new_string(message));
    sentry_value_set_by_key(event, "level", sentry_value_new_string("error"));
    
    // Add AST-specific context
    sentry_value_t ast_context = sentry_value_new_object();
    sentry_value_set_by_key(ast_context, "node_type", sentry_value_new_string(node_type));
    sentry_value_set_by_key(ast_context, "expected_type", sentry_value_new_int32(expected_type));
    sentry_value_set_by_key(ast_context, "actual_type", sentry_value_new_int32(actual_type));
    sentry_value_set_by_key(ast_context, "context", sentry_value_new_string(context));
    
    sentry_value_t contexts = sentry_value_new_object();
    sentry_value_set_by_key(contexts, "ast", ast_context);
    sentry_value_set_by_key(event, "contexts", contexts);
    
    // Add tags for easier filtering
    sentry_value_t tags = sentry_value_new_object();
    sentry_value_set_by_key(tags, "error_category", sentry_value_new_string("ast"));
    sentry_value_set_by_key(tags, "node_type", sentry_value_new_string(node_type));
    sentry_value_set_by_key(event, "tags", tags);
    
    sentry_capture_event(event);
}

// Report segmentation fault with context
void report_segfault(const char *function_name, const char *details) {
    sentry_value_t event = sentry_value_new_event();
    
    char message[512];
    snprintf(message, sizeof(message), 
             "Segmentation fault in %s: %s", function_name, details);
    
    sentry_value_set_by_key(event, "message", sentry_value_new_string(message));
    sentry_value_set_by_key(event, "level", sentry_value_new_string("fatal"));
    
    // Add crash context
    sentry_value_t crash_context = sentry_value_new_object();
    sentry_value_set_by_key(crash_context, "function", sentry_value_new_string(function_name));
    sentry_value_set_by_key(crash_context, "details", sentry_value_new_string(details));
    
    sentry_value_t contexts = sentry_value_new_object();
    sentry_value_set_by_key(contexts, "crash", crash_context);
    sentry_value_set_by_key(event, "contexts", contexts);
    
    // Add tags
    sentry_value_t tags = sentry_value_new_object();
    sentry_value_set_by_key(tags, "error_category", sentry_value_new_string("segfault"));
    sentry_value_set_by_key(tags, "function", sentry_value_new_string(function_name));
    sentry_value_set_by_key(event, "tags", tags);
    
    sentry_capture_event(event);
    
    // Ensure event is sent before potential crash
    sentry_flush(2000); // 2 second timeout
}

// Clean up Sentry
void cleanup_sentry() {
    sentry_close();
}