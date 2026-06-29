/*
 * Delta Module - Cryptographic Utilities
 * Part of Cipher-Core Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DELTA_VERSION "1.0.0"

typedef struct {
    unsigned char data[32];
    int len;
} DeltaHash;

static void delta_init(void) {
    printf("Delta cryptographic module initialized v%s\n", DELTA_VERSION);
}

static void delta_hash(const char *input, DeltaHash *out) {
    /* Simple hash simulation */
    unsigned int hash = 5381;
    int c;
    const char *str = input;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    memset(out->data, 0, 32);
    memcpy(out->data, &hash, sizeof(hash));
    out->len = 32;
}

static void delta_print_hash(const DeltaHash *h) {
    printf("Delta Hash: ");
    for (int i = 0; i < h->len; i++) {
        printf("%02x", h->data[i]);
    }
    printf("\n");
}
