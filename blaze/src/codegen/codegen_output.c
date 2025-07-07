// Code generation for Blaze output methods
// Generates x64 assembly for print, txt, out, fmt, and dyn

#include "blaze_internals.h"

// Forward declarations
extern void generate_identifier(CodeBuffer* buf, ASTNode* nodes, uint16_t node_idx,
                              SymbolTable* symbols, char* string_pool);

// Helper to emit string output using platform-aware print
static void emit_write_string(CodeBuffer* buf, volatile const char* str, uint32_t len) {
    // Use the platform-aware print function
    extern void emit_platform_print_string(CodeBuffer* buf, Platform platform, 
                                         volatile const char* str, uint32_t len);
    emit_platform_print_string(buf, buf->target_platform, str, len);
}

// Process print method - filters out brackets
static char* process_print_string(const char* input, char* buffer, uint32_t* out_len) {
    uint32_t j = 0;
    uint32_t i = 0;
    uint32_t len = 0;
    
    // Calculate length
    while (input[len]) len++;
    
    // Filter brackets
    while (i < len && j < 1024) {
        if (input[i] == '[') {
            // Skip until closing bracket
            while (i < len && input[i] != ']') i++;
            if (i < len) i++; // Skip the ]
            // Skip any space after bracket
            if (i < len && input[i] == ' ') i++;
        } else {
            buffer[j++] = input[i++];
        }
    }
    
    *out_len = j;
    return buffer;
}

// Process fmt method - interpret format codes
static char* process_fmt_string(const char* input, char* buffer, uint32_t* out_len) {
    uint32_t j = 0;
    uint32_t i = 0;
    uint32_t len = 0;
    
    // Calculate length
    while (input[len]) len++;
    
    // For now, simple implementation - just handle [bold]
    while (i < len && j < 1024) {
        if (input[i] == '[' && i + 5 < len && 
            input[i+1] == 'b' && input[i+2] == 'o' && 
            input[i+3] == 'l' && input[i+4] == 'd' && input[i+5] == ']') {
            // Add ANSI bold code
            buffer[j++] = '\033';
            buffer[j++] = '[';
            buffer[j++] = '1';
            buffer[j++] = 'm';
            i += 6; // Skip [bold]
        } else if (input[i] == '[' && i + 4 < len &&
                   input[i+1] == 'r' && input[i+2] == 'e' && 
                   input[i+3] == 'd' && input[i+4] == ']') {
            // Add ANSI red code
            buffer[j++] = '\033';
            buffer[j++] = '[';
            buffer[j++] = '3';
            buffer[j++] = '1';
            buffer[j++] = 'm';
            i += 5; // Skip [red]
        } else {
            buffer[j++] = input[i++];
        }
    }
    
    // Reset formatting at end
    buffer[j++] = '\033';
    buffer[j++] = '[';
    buffer[j++] = '0';
    buffer[j++] = 'm';
    
    *out_len = j;
    return buffer;
}

// Process out method - evaluate parameters
static char* process_out_string(const char* input, char* buffer, uint32_t* out_len, 
                               SymbolTable* symbols) {
    uint32_t j = 0;
    uint32_t i = 0;
    uint32_t len = 0;
    
    // Calculate length
    while (input[len]) len++;
    
    while (i < len && j < 1024) {
        if (input[i] == '{' && i + 1 < len && input[i+1] == '@') {
            // Found parameter reference
            i += 2; // Skip {@
            
            // Skip "param:"
            if (i + 6 < len && input[i] == 'p' && input[i+1] == 'a' && 
                input[i+2] == 'r' && input[i+3] == 'a' && input[i+4] == 'm' && 
                input[i+5] == ':') {
                i += 6;
            }
            
            // Extract parameter name
            char param_name[64];
            uint32_t name_len = 0;
            while (i < len && input[i] != '}' && name_len < 63) {
                param_name[name_len++] = input[i++];
            }
            param_name[name_len] = '\0';
            
            if (i < len && input[i] == '}') i++; // Skip }
            
            // Look up parameter value (simplified - just output the name for now)
            // In real implementation, would look up in symbol table
            for (uint32_t k = 0; k < name_len; k++) {
                buffer[j++] = param_name[k];
            }
        } else {
            buffer[j++] = input[i++];
        }
    }
    
    *out_len = j;
    return buffer;
}

// Main code generation for output methods
void gen_output_method(CodeBuffer* buf, ASTNode* node, 
                      char* string_pool, SymbolTable* symbols) {
    TokenType method = node->data.output.output_type;
    
    // Get the content string
    uint16_t content_idx = node->data.output.content_idx;
    char* content = "";
    
    if (content_idx != 0xFFFF) {
        // Since we stored the content in the string pool during parsing,
        // we can directly access it from there
        // The content_idx points to a STRING node
        content = &string_pool[content_idx];
        
        // Debug output
        const char* debug_msg = "DEBUG: print content='";
        emit_write_string(buf, debug_msg, 22);
        // Calculate content length
        uint32_t content_len = 0;
        while (content[content_len]) content_len++;
        emit_write_string(buf, content, content_len);
        emit_write_string(buf, "'\n", 2);
    }
    
    // Process based on output method
    char processed_buffer[1024];
    uint32_t processed_len = 0;
    
    switch (method) {
        case TOK_PRINT:
            // Filter out brackets
            process_print_string(content, processed_buffer, &processed_len);
            break;
            
        case TOK_TXT:
            // For txt/, we need to evaluate the content as a variable or expression
            // The content should be a variable name or expression that we evaluate
            if (content_idx != 0xFFFF) {
                // The content_idx points to a node that contains the variable name
                // We need to generate code to load the variable value and convert it to string
                
                // For now, let's generate code to load the variable and print its value
                // This is a simplified approach - in a full implementation, we'd convert the value to string
                
                // Create a temporary identifier node for the variable
                ASTNode temp_node;
                temp_node.type = NODE_IDENTIFIER;
                temp_node.data.ident.name_offset = content_idx;
                temp_node.data.ident.name_len = 0;
                while (content[temp_node.data.ident.name_len]) {
                    temp_node.data.ident.name_len++;
                }
                
                // Generate code to load the variable value into RAX
                generate_identifier(buf, &temp_node, 0, symbols, string_pool);
                
                // For now, just output the variable name as a placeholder
                // TODO: Implement proper value-to-string conversion
                processed_len = 0;
                while (content[processed_len] && processed_len < 1024) {
                    processed_buffer[processed_len] = content[processed_len];
                    processed_len++;
                }
                
                // Add a debug message to show we're processing txt command
                const char* debug_msg = "DEBUG: txt command processed for variable: ";
                emit_write_string(buf, debug_msg, 38);
                emit_write_string(buf, content, processed_len);
                emit_write_string(buf, "\n", 1);
            } else {
                // No content - output empty string
                processed_len = 0;
            }
            break;
            
        case TOK_OUT:
            // Evaluate parameters
            process_out_string(content, processed_buffer, &processed_len, symbols);
            break;
            
        case TOK_FMT:
            // Apply formatting
            process_fmt_string(content, processed_buffer, &processed_len);
            break;
            
        case TOK_DYN:
            // For now, just output if no condition (simplified)
            // Real implementation would evaluate [if condition]
            processed_len = 0;
            while (content[processed_len] && processed_len < 1024) {
                processed_buffer[processed_len] = content[processed_len];
                processed_len++;
            }
            break;
            
        default:
            return;
    }
    
    // Add newline if not present
    if (processed_len > 0 && processed_buffer[processed_len-1] != '\n') {
        processed_buffer[processed_len++] = '\n';
    }
    
    // Store processed string in data section
    // In real implementation, this would be in .data section
    static volatile char output_strings[65536];
    static volatile uint32_t string_offset = 0;
    
    uint32_t str_addr_offset = string_offset;
    for (uint32_t i = 0; i < processed_len; i++) {
        output_strings[string_offset++] = processed_buffer[i];
    }
    
    // Handle chained output with underscore connector
    if (node->data.output.next_output != 0xFFFF) {
        // For chained output, we need to pass the processed result
        // to the next output method instead of writing it directly
        
        // Store current result in string pool for next method
        uint32_t chain_offset = 0x1000; // Temporary storage area
        char* chain_buffer = &output_strings[chain_offset];
        
        // Copy processed buffer to chain storage
        for (uint32_t i = 0; i < processed_len; i++) {
            chain_buffer[i] = processed_buffer[i];
        }
        chain_buffer[processed_len] = '\0';
        
        // Create a temporary node for the next output method
        ASTNode temp_node = *node;
        temp_node.data.output.output_type = node->data.output.next_output;
        temp_node.data.output.content_idx = chain_offset;
        temp_node.data.output.next_output = 0xFFFF; // Prevent infinite recursion
        
        // Process the next output method with our result
        gen_output_method(buf, &temp_node, chain_buffer, symbols);
    } else {
        // No chaining - emit the output directly
        emit_write_string(buf, &output_strings[str_addr_offset], processed_len);
    }
}