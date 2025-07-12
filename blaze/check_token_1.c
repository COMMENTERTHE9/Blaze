
#include "include/blaze_internals.h"
#include <stdio.h>
int main() { 
    printf("Token type 1 = ");
    for(int i = 0; i < 256; i++) {
        if(i == 1) {
            printf("TOK_GT (>)\n");
            break;
        }
    }
    return 0; 
}
