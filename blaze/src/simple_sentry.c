#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "simple_sentry.h"

// Simple file-based error logging that can be read by Sentry MCP
static FILE* error_log = NULL;
static char session_id[64];

void init_simple_sentry() {
    // Generate session ID
    srand(time(NULL));
    snprintf(session_id, sizeof(session_id), "blaze_%ld_%d", time(NULL), rand());
    
    // Open error log
    error_log = fopen("blaze_errors.log", "a");
    if (error_log) {
        fprintf(error_log, "\n=== New Session: %s ===\n", session_id);
        fprintf(error_log, "Timestamp: %ld\n", time(NULL));
        fflush(error_log);
    }
}

void report_error(const char* type, const char* message, const char* file, int line) {
    if (!error_log) return;
    
    fprintf(error_log, "[ERROR] %s: %s (at %s:%d)\n", type, message, file, line);
    fprintf(error_log, "  Session: %s\n", session_id);
    fprintf(error_log, "  Time: %ld\n", time(NULL));
    fflush(error_log);
    
    // Also print to stderr
    fprintf(stderr, "BLAZE ERROR: %s: %s (at %s:%d)\n", type, message, file, line);
}

void add_breadcrumb(const char* category, const char* message) {
    if (!error_log) return;
    
    fprintf(error_log, "[BREADCRUMB] %s: %s\n", category, message);
    fflush(error_log);
}

void report_ast_error(const char* node_type, int expected, int actual) {
    char message[256];
    snprintf(message, sizeof(message), 
             "AST type mismatch for %s: expected %d, got %d", 
             node_type, expected, actual);
    report_error("AST_ERROR", message, __FILE__, __LINE__);
}

void cleanup_simple_sentry() {
    if (error_log) {
        fprintf(error_log, "=== Session End: %s ===\n\n", session_id);
        fclose(error_log);
        error_log = NULL;
    }
}