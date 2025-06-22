#ifndef SENTRY_INTEGRATION_H
#define SENTRY_INTEGRATION_H

// Initialize Sentry SDK
void init_sentry(void);

// Report compiler errors
void report_compiler_error(const char *error_type, const char *message, const char *file, int line);

// Add breadcrumbs for tracking compilation steps
void add_compilation_breadcrumb(const char *step, const char *details);

// Report AST node type mismatches
void report_ast_error(const char *node_type, int expected_type, int actual_type, const char *context);

// Report segmentation faults with context
void report_segfault(const char *function_name, const char *details);

// Clean up Sentry (call before exit)
void cleanup_sentry(void);

// Convenience macros for error reporting
#define REPORT_ERROR(type, msg) report_compiler_error(type, msg, __FILE__, __LINE__)
#define REPORT_SEGFAULT(details) report_segfault(__func__, details)
#define TRACK_COMPILATION_STEP(step, details) add_compilation_breadcrumb(step, details)

#endif // SENTRY_INTEGRATION_H