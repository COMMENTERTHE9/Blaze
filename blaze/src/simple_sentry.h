#ifndef SIMPLE_SENTRY_H
#define SIMPLE_SENTRY_H

// Simple error reporting that can be integrated with Sentry MCP

// Initialize error logging
void init_simple_sentry(void);

// Report an error
void report_error(const char* type, const char* message, const char* file, int line);

// Add breadcrumb
void add_breadcrumb(const char* category, const char* message);

// Report AST errors
void report_ast_error(const char* node_type, int expected, int actual);

// Cleanup
void cleanup_simple_sentry(void);

// HTTP-enabled versions that send to Sentry.io
void init_sentry_http(void);
void report_error_with_sentry(const char* type, const char* message, const char* file, int line);
void send_breadcrumb_to_sentry(const char* category, const char* message);

// Convenience macros - now with HTTP support
#define SENTRY_INIT() init_sentry_http()
#define SENTRY_ERROR(type, msg) report_error_with_sentry(type, msg, __FILE__, __LINE__)
#define SENTRY_BREADCRUMB(cat, msg) send_breadcrumb_to_sentry(cat, msg)
#define SENTRY_CLEANUP() cleanup_simple_sentry()

#endif // SIMPLE_SENTRY_H