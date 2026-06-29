/*
 * Lambda Module - Compression Utilities
 * Part of Cipher-Core Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LAMBDA_VERSION "1.0.0"
#define COMPRESS_BUFFER_SIZE 4096

static void lambda_init(void) {
    printf("Lambda compression utilities initialized v%s\n", LAMBDA_VERSION);
}

/* Simple RLE compression simulation */
static int lambda_compress(const char *input, char *output, int max_len) {
    int in_len = strlen(input);
    int out_pos = 0;
    
    for (int i = 0; i < in_len && out_pos < max_len - 1; i++) {
        int count = 1;
        while (i + count < in_len && input[i] == input[i + count] && count < 255) {
            count++;
        }
        output[out_pos++] = (char)count;
        output[out_pos++] = input[i];
        i += count - 1;
    }
    output[out_pos] = '\0';
    return out_pos;
}

static int lambda_decompress(const char *input, char *output, int max_len) {
    int in_len = strlen(input);
    int out_pos = 0;
    
    for (int i = 0; i < in_len && out_pos < max_len - 1; i += 2) {
        int count = (unsigned char)input[i];
        char ch = input[i+1];
        for (int j = 0; j < count && out_pos < max_len - 1; j++) {
            output[out_pos++] = ch;
        }
    }
    output[out_pos] = '\0';
    return out_pos;
}
