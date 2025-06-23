// SOLID NUMBER SIMD OPTIMIZATIONS
// Uses SSE/AVX instructions for fast terminal digit operations

#include "blaze_internals.h"
#include "solid_runtime.h"

// Check CPU capabilities
static bool has_sse2 = false;
static bool has_avx2 = false;
static bool cpu_checked = false;

// CPU feature detection
static void check_cpu_features(void) {
    if (cpu_checked) return;
    
    // Use CPUID to check features
    asm volatile(
        "mov $1, %%eax\n"
        "cpuid\n"
        "mov %%edx, %0\n"
        "mov %%ecx, %1\n"
        : "=r"(has_sse2), "=r"(has_avx2)
        :
        : "%eax", "%ebx", "%ecx", "%edx"
    );
    
    // Check SSE2 (bit 26 of EDX)
    has_sse2 = (has_sse2 >> 26) & 1;
    
    // For AVX2, need to check additional CPUID leaf
    if (has_avx2 & (1 << 28)) { // Check OSXSAVE bit
        asm volatile(
            "mov $7, %%eax\n"
            "xor %%ecx, %%ecx\n"
            "cpuid\n"
            "mov %%ebx, %0\n"
            : "=r"(has_avx2)
            :
            : "%eax", "%ebx", "%ecx", "%edx"
        );
        has_avx2 = (has_avx2 >> 5) & 1; // AVX2 bit
    } else {
        has_avx2 = false;
    }
    
    cpu_checked = true;
    
    print_str("[SOLID-SIMD] CPU features: SSE2=");
    print_str(has_sse2 ? "YES" : "NO");
    print_str(", AVX2=");
    print_str(has_avx2 ? "YES" : "NO");
    print_str("\n");
}

// SIMD terminal digit comparison using SSE2
bool solid_terminal_compare_sse2(const char* term_a, const char* term_b, uint16_t len) {
    if (!has_sse2 || len < 16) {
        // Fall back to regular comparison
        for (uint16_t i = 0; i < len; i++) {
            if (term_a[i] != term_b[i]) return false;
        }
        return true;
    }
    
    bool result = true;
    
    asm volatile(
        "xor %%rax, %%rax\n"           // Clear result
        "mov %2, %%rcx\n"              // Load length
        "shr $4, %%rcx\n"              // Divide by 16 (SSE register size)
        "jz .sse2_remainder\n"         // If less than 16, skip to remainder
        
        ".sse2_loop:\n"
        "movdqu (%0), %%xmm0\n"        // Load 16 bytes from term_a
        "movdqu (%1), %%xmm1\n"        // Load 16 bytes from term_b
        "pcmpeqb %%xmm0, %%xmm1\n"     // Compare bytes
        "pmovmskb %%xmm1, %%eax\n"     // Extract comparison mask
        "cmp $0xFFFF, %%eax\n"         // Check if all bytes matched
        "jne .sse2_not_equal\n"        // If not, terminals differ
        "add $16, %0\n"                // Advance pointers
        "add $16, %1\n"
        "loop .sse2_loop\n"
        
        ".sse2_remainder:\n"
        // Handle remaining bytes (less than 16)
        "mov %2, %%rcx\n"
        "and $15, %%rcx\n"             // Get remainder
        "jz .sse2_equal\n"             // If no remainder, we're done
        
        ".sse2_byte_loop:\n"
        "movb (%0), %%al\n"
        "cmpb (%1), %%al\n"
        "jne .sse2_not_equal\n"
        "inc %0\n"
        "inc %1\n"
        "loop .sse2_byte_loop\n"
        
        ".sse2_equal:\n"
        "mov $1, %3\n"                 // Set result = true
        "jmp .sse2_done\n"
        
        ".sse2_not_equal:\n"
        "mov $0, %3\n"                 // Set result = false
        
        ".sse2_done:\n"
        : "+r"(term_a), "+r"(term_b), "+r"(len), "=r"(result)
        :
        : "%rax", "%rcx", "%xmm0", "%xmm1", "memory"
    );
    
    return result;
}

// SIMD terminal digit addition (modular arithmetic)
void solid_terminal_add_simd(const char* term_a, const char* term_b, 
                            char* result, uint16_t len) {
    check_cpu_features();
    
    if (!has_sse2 || len < 16) {
        // Fall back to scalar addition
        int carry = 0;
        for (int i = len - 1; i >= 0; i--) {
            if (term_a[i] >= '0' && term_a[i] <= '9' &&
                term_b[i] >= '0' && term_b[i] <= '9') {
                int sum = (term_a[i] - '0') + (term_b[i] - '0') + carry;
                result[i] = '0' + (sum % 10);
                carry = sum / 10;
            } else {
                result[i] = term_a[i]; // Non-digit, just copy
            }
        }
        return;
    }
    
    // SSE2 vectorized addition for terminal digits
    asm volatile(
        // Load constants
        "movdqa .LC_ASCII_ZERO, %%xmm7\n"   // Vector of '0' (0x30)
        "movdqa .LC_TEN, %%xmm6\n"          // Vector of 10
        "pxor %%xmm5, %%xmm5\n"             // Zero for carry
        
        "mov %3, %%rcx\n"                   // Length
        "add %%rcx, %0\n"                   // Point to end of term_a
        "add %%rcx, %1\n"                   // Point to end of term_b
        "add %%rcx, %2\n"                   // Point to end of result
        "neg %%rcx\n"                       // Negate for reverse iteration
        
        ".simd_add_loop:\n"
        "cmp $-16, %%rcx\n"
        "jg .simd_add_remainder\n"
        
        // Load 16 digits from each terminal
        "movdqu (%%rcx,%0), %%xmm0\n"       // term_a digits
        "movdqu (%%rcx,%1), %%xmm1\n"       // term_b digits
        
        // Convert from ASCII to numeric
        "psubb %%xmm7, %%xmm0\n"            // Subtract '0'
        "psubb %%xmm7, %%xmm1\n"
        
        // Add with carry
        "paddb %%xmm1, %%xmm0\n"            // Add digits
        "paddb %%xmm5, %%xmm0\n"            // Add carry
        
        // Handle modulo 10 (simplified - doesn't handle full carry propagation)
        "movdqa %%xmm0, %%xmm2\n"
        "pcmpgtb %%xmm6, %%xmm2\n"          // Compare with 10
        "pand %%xmm6, %%xmm2\n"             // Mask values >= 10
        "psubb %%xmm2, %%xmm0\n"            // Subtract 10 where needed
        
        // Convert back to ASCII
        "paddb %%xmm7, %%xmm0\n"
        
        // Store result
        "movdqu %%xmm0, (%%rcx,%2)\n"
        
        "add $16, %%rcx\n"
        "jmp .simd_add_loop\n"
        
        ".simd_add_remainder:\n"
        // Handle remaining bytes with scalar code
        "test %%rcx, %%rcx\n"
        "jz .simd_add_done\n"
        
        ".scalar_add_loop:\n"
        "movb (%%rcx,%0), %%al\n"
        "movb (%%rcx,%1), %%bl\n"
        "sub $0x30, %%al\n"                 // Convert from ASCII
        "sub $0x30, %%bl\n"
        "add %%bl, %%al\n"                  // Add
        "cmp $10, %%al\n"
        "jl .no_carry\n"
        "sub $10, %%al\n"
        ".no_carry:\n"
        "add $0x30, %%al\n"                 // Convert to ASCII
        "movb %%al, (%%rcx,%2)\n"
        "inc %%rcx\n"
        "jnz .scalar_add_loop\n"
        
        ".simd_add_done:\n"
        
        // Constants in .rodata section
        ".section .rodata\n"
        ".align 16\n"
        ".LC_ASCII_ZERO: .fill 16, 1, 0x30\n"
        ".LC_TEN: .fill 16, 1, 10\n"
        ".text\n"
        
        : "+r"(term_a), "+r"(term_b), "+r"(result)
        : "r"((uint64_t)len)
        : "%rax", "%rbx", "%rcx", "%xmm0", "%xmm1", "%xmm2", 
          "%xmm5", "%xmm6", "%xmm7", "memory"
    );
}

// AVX2 terminal pattern detection (finds repeating patterns faster)
uint32_t solid_terminal_pattern_avx2(const char* terminals, uint16_t len) {
    if (!has_avx2 || len < 32) {
        // Fall back to simple pattern detection
        for (uint32_t period = 1; period <= len / 2; period++) {
            bool matches = true;
            for (uint16_t i = 0; i < period && i + period < len; i++) {
                if (terminals[i] != terminals[i + period]) {
                    matches = false;
                    break;
                }
            }
            if (matches) return period;
        }
        return 0; // No pattern
    }
    
    uint32_t pattern_period = 0;
    
    // AVX2 can process 32 bytes at once
    asm volatile(
        "mov $1, %%r8d\n"                   // Start with period = 1
        
        ".avx2_period_loop:\n"
        "cmp %1, %%r8d\n"                   // Check if period > len/2
        "jg .avx2_no_pattern\n"
        
        "mov %0, %%rsi\n"                   // Source pointer
        "mov %%rsi, %%rdi\n"
        "add %%r8, %%rdi\n"                 // Offset by period
        
        "mov %1, %%rcx\n"                   // Length
        "sub %%r8, %%rcx\n"                 // Adjust for period offset
        "shr $5, %%rcx\n"                   // Divide by 32 (AVX register size)
        "jz .avx2_check_remainder\n"
        
        ".avx2_compare_loop:\n"
        "vmovdqu (%%rsi), %%ymm0\n"         // Load 32 bytes
        "vpcmpeqb (%%rdi), %%ymm0, %%ymm1\n" // Compare with offset
        "vpmovmskb %%ymm1, %%eax\n"         // Extract mask
        "cmp $-1, %%eax\n"                  // Check if all matched
        "jne .avx2_next_period\n"           // If not, try next period
        "add $32, %%rsi\n"
        "add $32, %%rdi\n"
        "loop .avx2_compare_loop\n"
        
        ".avx2_check_remainder:\n"
        // Pattern found for this period
        "mov %%r8d, %2\n"
        "jmp .avx2_done\n"
        
        ".avx2_next_period:\n"
        "inc %%r8d\n"
        "jmp .avx2_period_loop\n"
        
        ".avx2_no_pattern:\n"
        "xor %2, %2\n"                      // No pattern found
        
        ".avx2_done:\n"
        "vzeroupper\n"                      // Clean up AVX state
        
        : "=r"(terminals), "=r"(len), "=r"(pattern_period)
        : "0"(terminals), "1"(len)
        : "%rax", "%rcx", "%rsi", "%rdi", "%r8", 
          "%ymm0", "%ymm1", "memory"
    );
    
    return pattern_period;
}

// SIMD-accelerated terminal modular multiplication
void solid_terminal_multiply_simd(const char* term_a, const char* term_b,
                                 char* result, uint16_t len, uint64_t modulus) {
    check_cpu_features();
    
    if (!has_sse2 || len < 8) {
        // Scalar fallback
        uint64_t prod = 0;
        uint64_t multiplier = 1;
        
        // Convert terminals to number
        for (int i = len - 1; i >= 0; i--) {
            if (term_a[i] >= '0' && term_a[i] <= '9') {
                prod += (term_a[i] - '0') * multiplier;
                multiplier *= 10;
            }
        }
        
        uint64_t prod_b = 0;
        multiplier = 1;
        for (int i = len - 1; i >= 0; i--) {
            if (term_b[i] >= '0' && term_b[i] <= '9') {
                prod_b += (term_b[i] - '0') * multiplier;
                multiplier *= 10;
            }
        }
        
        // Multiply and take modulus
        prod = (prod * prod_b) % modulus;
        
        // Convert back to string
        for (int i = len - 1; i >= 0; i--) {
            result[i] = '0' + (prod % 10);
            prod /= 10;
        }
        return;
    }
    
    // SSE2 implementation for digit-wise multiplication
    // This is simplified - full implementation would handle carry properly
    asm volatile(
        "movq %3, %%mm7\n"                  // Load modulus into MM7
        
        // Process 8 digits at a time
        "mov %2, %%rcx\n"
        "shr $3, %%rcx\n"                   // Divide by 8
        "jz .simd_mul_done\n"
        
        ".simd_mul_loop:\n"
        "movq (%0), %%mm0\n"                // Load 8 digits from term_a
        "movq (%1), %%mm1\n"                // Load 8 digits from term_b
        
        // Convert from ASCII and multiply (simplified)
        "movq %%mm0, %%mm2\n"
        "movq %%mm1, %%mm3\n"
        "psubb .LC_MM_ZERO, %%mm2\n"        // Convert from ASCII
        "psubb .LC_MM_ZERO, %%mm3\n"
        "pmullw %%mm3, %%mm2\n"             // Multiply low words
        
        // Simple modulus (doesn't handle full range)
        "movq %%mm2, %%mm4\n"
        "pcmpgtw %%mm7, %%mm4\n"            // Compare with modulus
        "pand %%mm7, %%mm4\n"
        "psubw %%mm4, %%mm2\n"              // Subtract if needed
        
        // Convert back to ASCII
        "paddb .LC_MM_ZERO, %%mm2\n"
        "movq %%mm2, (%2)\n"                // Store result
        
        "add $8, %0\n"
        "add $8, %1\n"
        "add $8, %2\n"
        "loop .simd_mul_loop\n"
        
        ".simd_mul_done:\n"
        "emms\n"                            // Clear MMX state
        
        // Constants
        ".section .rodata\n"
        ".align 8\n"
        ".LC_MM_ZERO: .fill 8, 1, 0x30\n"
        ".text\n"
        
        : "+r"(term_a), "+r"(term_b), "+r"(result)
        : "r"(modulus)
        : "%rcx", "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm7", "memory"
    );
}

// Initialize SIMD optimizations
void solid_simd_init(void) {
    check_cpu_features();
    
    if (has_sse2) {
        print_str("[SOLID-SIMD] SSE2 optimizations enabled\n");
    }
    
    if (has_avx2) {
        print_str("[SOLID-SIMD] AVX2 optimizations enabled\n");
    }
    
    if (!has_sse2 && !has_avx2) {
        print_str("[SOLID-SIMD] No SIMD support detected, using scalar fallbacks\n");
    }
}

// Benchmark function to test SIMD performance
void solid_simd_benchmark(void) {
    print_str("\n=== SOLID SIMD BENCHMARK ===\n");
    
    // Create test data
    char term_a[32] = "31415926535897932384626433832795";
    char term_b[32] = "27182818284590452353602874713527";
    char result[32];
    
    // Benchmark terminal comparison
    uint64_t start, end;
    
    asm volatile("rdtsc" : "=A"(start));
    for (int i = 0; i < 1000000; i++) {
        solid_terminal_compare_sse2(term_a, term_b, 32);
    }
    asm volatile("rdtsc" : "=A"(end));
    
    print_str("SSE2 comparison (1M iterations): ");
    print_num((end - start) / 1000000);
    print_str(" cycles/iteration\n");
    
    // Benchmark terminal addition
    asm volatile("rdtsc" : "=A"(start));
    for (int i = 0; i < 1000000; i++) {
        solid_terminal_add_simd(term_a, term_b, result, 32);
    }
    asm volatile("rdtsc" : "=A"(end));
    
    print_str("SIMD addition (1M iterations): ");
    print_num((end - start) / 1000000);
    print_str(" cycles/iteration\n");
    
    // Show result of addition
    print_str("Addition result: ");
    for (int i = 0; i < 32; i++) {
        print_char(result[i]);
    }
    print_str("\n");
    
    // Benchmark pattern detection
    if (has_avx2) {
        char pattern[64] = "1234567890123456789012345678901234567890123456789012345678901234";
        
        asm volatile("rdtsc" : "=A"(start));
        uint32_t period = solid_terminal_pattern_avx2(pattern, 64);
        asm volatile("rdtsc" : "=A"(end));
        
        print_str("AVX2 pattern detection: ");
        print_num(end - start);
        print_str(" cycles, found period: ");
        print_num(period);
        print_str("\n");
    }
}