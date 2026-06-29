/*
 * Utility Functions for Cipher-Core
 */

#include "common.h"
#include <time.h>

void *safe_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void safe_free(void *ptr) {
    if (ptr) {
        free(ptr);
    }
}

uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void hexdump(const void *data, size_t len) {
    const unsigned char *bytes = data;
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", bytes[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (len % 16 != 0) printf("\n");
}
