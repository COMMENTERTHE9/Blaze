#include <stdio.h>
#include <string.h>

int main() {
    const char* test = "3.14159...(q:10^35|0.85)...787";
    int pos = 0;
    int len = strlen(test);
    
    printf("Testing solid number parsing:\n");
    printf("Input: %s\n", test);
    printf("Length: %d\n", len);
    
    // Simulate number parsing
    while (pos < len && ((test[pos] >= '0' && test[pos] <= '9') || test[pos] == '.')) {
        printf("pos=%d char='%c'\n", pos, test[pos]);
        pos++;
    }
    
    printf("\nAfter number parsing, pos=%d\n", pos);
    
    if (pos + 2 < len) {
        printf("Next 3 chars: '%c' '%c' '%c'\n", test[pos], test[pos+1], test[pos+2]);
        if (test[pos] == '.' && test[pos+1] == '.' && test[pos+2] == '.') {
            printf("Found solid number pattern!\n");
        }
    }
    
    return 0;
}