#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include "simple_sentry.h"

// Sentry DSN components
#define SENTRY_KEY "903718515ee95abc1f9b4b5c4752461b"
#define SENTRY_PROJECT "4509528390369280"
#define SENTRY_HOST "o4509528354390016.ingest.us.sentry.io"

// Send event to Sentry using curl
static void send_to_sentry(const char* level, const char* message, const char* logger) {
    // Create timestamp
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    
    // Create event ID (simple random)
    char event_id[33];
    for (int i = 0; i < 32; i++) {
        event_id[i] = "0123456789abcdef"[rand() % 16];
    }
    event_id[32] = '\0';
    
    // Build JSON payload
    char json[2048];
    snprintf(json, sizeof(json),
        "{"
        "\"event_id\":\"%s\","
        "\"message\":\"%s\","
        "\"timestamp\":\"%s\","
        "\"level\":\"%s\","
        "\"logger\":\"%s\","
        "\"platform\":\"native\","
        "\"release\":\"blaze-compiler@0.1.0\","
        "\"environment\":\"development\","
        "\"tags\":{"
            "\"compiler\":\"blaze\","
            "\"arch\":\"x86_64\""
        "}"
        "}",
        event_id, message, timestamp, level, logger
    );
    
    // Use curl to send the event
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - run curl silently
        char url[256];
        snprintf(url, sizeof(url), 
                "https://%s/api/%s/store/", 
                SENTRY_HOST, SENTRY_PROJECT);
        
        char auth_header[256];
        snprintf(auth_header, sizeof(auth_header),
                "X-Sentry-Auth: Sentry sentry_version=7, sentry_key=%s",
                SENTRY_KEY);
        
        // Redirect output to /dev/null to run silently
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        
        execlp("curl", "curl",
               "-X", "POST",
               url,
               "-H", "Content-Type: application/json",
               "-H", auth_header,
               "-d", json,
               "--silent",
               "--max-time", "2",  // 2 second timeout
               NULL);
        
        // If exec fails, exit child
        exit(1);
    } else if (pid > 0) {
        // Parent process - don't wait, continue immediately
        // This makes it non-blocking
    }
}

// Enhanced error reporting that sends to Sentry
void report_error_with_sentry(const char* type, const char* message, const char* file, int line) {
    // Log locally first
    report_error(type, message, file, line);
    
    // Build full message
    char full_message[512];
    snprintf(full_message, sizeof(full_message), 
             "%s: %s (at %s:%d)", type, message, file, line);
    
    // Send to Sentry
    send_to_sentry("error", full_message, "blaze");
}

// Send breadcrumb to Sentry
void send_breadcrumb_to_sentry(const char* category, const char* message) {
    // For breadcrumbs, we'll batch them and send with the next error
    // For now, just log locally
    add_breadcrumb(category, message);
}

// Initialize with Sentry HTTP support
void init_sentry_http() {
    // Initialize local logging
    init_simple_sentry();
    
    // Send startup event to Sentry
    send_to_sentry("info", "Blaze compiler started with Sentry integration", "blaze");
}