// Updated timeline parser to handle bounce, merge, and queue syntax

#include "blaze_internals.h"

// Parser structure is defined in blaze_internals.h
// Function forward declarations:
extern bool at_end(Parser* p);
extern Token* peek(Parser* p);
extern Token* advance(Parser* p);
extern bool check(Parser* p, TokenType type);
extern bool match(Parser* p, TokenType type);
extern uint16_t alloc_node(Parser* p, NodeType type);
extern uint32_t store_string(Parser* p, Token* tok);
extern uint16_t parse_expression(Parser* p);
extern uint16_t parse_identifier(Parser* p);

// Parse timeline with bounce syntax:
// ^timeline.[|processor|.state_1 bnc unwanted_timeline recv]/
static uint16_t parse_timeline_jump_with_bounce(Parser* p) {
    // Already consumed TOK_TIMELINE_JUMP
    
    uint16_t timeline_node = alloc_node(p, NODE_JUMP);
    if (timeline_node == 0xFFFF) return 0xFFFF;
    
    // The lexer gives us "^timeline.[" as one token
    // Now we need to parse the content until ]
    
    // Parse target state (e.g., |processor|.state_1)
    uint16_t target_expr = parse_expression(p);
    if (target_expr == 0xFFFF) {
        p->has_error = true;
        return 0xFFFF;
    }
    
    p->nodes[timeline_node].data.timing.expr_idx = target_expr;
    
    // Check for bounce syntax
    if (check(p, TOK_BNC)) {
        advance(p); // consume 'bnc'
        
        // Parse timeline to bounce
        uint16_t bounce_target = parse_identifier(p);
        if (bounce_target == 0xFFFF) {
            p->has_error = true;
            return 0xFFFF;
        }
        
        // Expect 'recv'
        if (!match(p, TOK_RECV)) {
            p->has_error = true;
            return 0xFFFF;
        }
        
        // Store bounce info in the node
        // Using temporal_offset to store bounce target (hacky but works)
        p->nodes[timeline_node].data.timing.temporal_offset = bounce_target;
        p->nodes[timeline_node].data.timing.timing_op = TOK_BNC; // Mark as bounce operation
    }
    
    // Expect closing ]
    if (!match(p, TOK_BRACKET_CLOSE)) {
        p->has_error = true;
        return 0xFFFF;
    }
    
    // Expect /
    if (!match(p, TOK_SLASH)) {
        p->has_error = true;
        return 0xFFFF;
    }
    
    return timeline_node;
}

// Parse recv._merg syntax:
// func.can/ recv._merg/{@param:timeline_a}/{@param:timeline_b} /
static uint16_t parse_recv_merge(Parser* p) {
    // Check for recv._merg pattern
    Token* recv_tok = peek(p);
    if (!recv_tok || recv_tok->type != TOK_RECV) return 0xFFFF;
    
    // Look ahead for ._merg
    if (p->current + 1 < p->count && 
        p->tokens[p->current + 1].type == TOK_DOT) {
        
        // Save position for potential backtrack
        uint32_t saved_pos = p->current;
        
        advance(p); // consume 'recv'
        advance(p); // consume '.'
        
        // Check for _merg
        if (check(p, TOK_IDENTIFIER)) {
            Token* merg_tok = peek(p);
            if (merg_tok->len == 5 && 
                p->source[merg_tok->start] == '_' &&
                p->source[merg_tok->start + 1] == 'm' &&
                p->source[merg_tok->start + 2] == 'e' &&
                p->source[merg_tok->start + 3] == 'r' &&
                p->source[merg_tok->start + 4] == 'g') {
                
                advance(p); // consume '_merg'
                
                // Create a merge node
                uint16_t merge_node = alloc_node(p, NODE_TIMING_OP);
                if (merge_node == 0xFFFF) return 0xFFFF;
                
                p->nodes[merge_node].data.timing.timing_op = TOK_RECV; // Mark as recv operation
                
                // Parse parameters
                if (match(p, TOK_SLASH)) {
                    // First parameter
                    if (check(p, TOK_PARAM)) {
                        uint16_t param1 = parse_expression(p);
                        p->nodes[merge_node].data.timing.expr_idx = param1;
                    }
                    
                    if (match(p, TOK_SLASH)) {
                        // Second parameter
                        if (check(p, TOK_PARAM)) {
                            uint16_t param2 = parse_expression(p);
                            p->nodes[merge_node].data.timing.temporal_offset = param2;
                        }
                    }
                }
                
                return merge_node;
            }
        }
        
        // Not a merge, backtrack
        p->current = saved_pos;
    }
    
    return 0xFFFF;
}

// Parse recv._queue syntax:
// func.can/ recv._queue/{@param:timeline_first}/{@param:timeline_second} /
static uint16_t parse_recv_queue(Parser* p) {
    // Similar to parse_recv_merge but looking for _queue
    Token* recv_tok = peek(p);
    if (!recv_tok || recv_tok->type != TOK_RECV) return 0xFFFF;
    
    // Look ahead for ._queue
    if (p->current + 1 < p->count && 
        p->tokens[p->current + 1].type == TOK_DOT) {
        
        uint32_t saved_pos = p->current;
        
        advance(p); // consume 'recv'
        advance(p); // consume '.'
        
        // Check for _queue
        if (check(p, TOK_IDENTIFIER)) {
            Token* queue_tok = peek(p);
            if (queue_tok->len == 6 && 
                p->source[queue_tok->start] == '_' &&
                p->source[queue_tok->start + 1] == 'q' &&
                p->source[queue_tok->start + 2] == 'u' &&
                p->source[queue_tok->start + 3] == 'e' &&
                p->source[queue_tok->start + 4] == 'u' &&
                p->source[queue_tok->start + 5] == 'e') {
                
                advance(p); // consume '_queue'
                
                // Create a queue node
                uint16_t queue_node = alloc_node(p, NODE_TIMING_OP);
                if (queue_node == 0xFFFF) return 0xFFFF;
                
                // Use a different marker to distinguish from merge
                p->nodes[queue_node].data.timing.timing_op = TOK_RECV + 1; // Hack: RECV+1 means queue
                
                // Parse parameters
                if (match(p, TOK_SLASH)) {
                    if (check(p, TOK_PARAM)) {
                        uint16_t param1 = parse_expression(p);
                        p->nodes[queue_node].data.timing.expr_idx = param1;
                    }
                    
                    if (match(p, TOK_SLASH)) {
                        if (check(p, TOK_PARAM)) {
                            uint16_t param2 = parse_expression(p);
                            p->nodes[queue_node].data.timing.temporal_offset = param2;
                        }
                    }
                }
                
                return queue_node;
            }
        }
        
        // Not a queue, backtrack
        p->current = saved_pos;
    }
    
    return 0xFFFF;
}

// Updated timeline parser that handles all cases
uint16_t parse_timeline_enhanced(Parser* p) {
    if (check(p, TOK_TIMELINE_DEF)) {
        // Regular timeline definition: timeline-[name]
        advance(p);
        
        uint16_t timeline_node = alloc_node(p, NODE_JUMP);
        if (timeline_node == 0xFFFF) return 0xFFFF;
        
        // Parse timeline name/content
        while (!at_end(p) && !check(p, TOK_BRACKET_CLOSE)) {
            advance(p);
        }
        match(p, TOK_BRACKET_CLOSE);
        
        return timeline_node;
    }
    else if (check(p, TOK_TIMELINE_JUMP)) {
        // Timeline jump with potential bounce
        advance(p);
        return parse_timeline_jump_with_bounce(p);
    }
    
    return 0xFFFF;
}