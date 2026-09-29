/*
 * Chi Module - Circular Ring Buffer
 * Part of Cipher-Core Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define CHI_VERSION "1.0.0"

typedef struct {
    uint8_t *buffer;
    size_t head;
    size_t tail;
    size_t size;
    size_t capacity;
} RingBuffer;

static RingBuffer *chi_ring_create(size_t capacity) {
    RingBuffer *rb = (RingBuffer *)malloc(sizeof(RingBuffer));
    if (!rb) return NULL;
    rb->buffer = (uint8_t *)malloc(capacity);
    rb->capacity = capacity;
    rb->head = 0;
    rb->tail = 0;
    rb->size = 0;
    return rb;
}

static bool chi_ring_push(RingBuffer *rb, uint8_t byte) {
    if (!rb || rb->size >= rb->capacity) return false;
    rb->buffer[rb->head] = byte;
    rb->head = (rb->head + 1) % rb->capacity;
    rb->size++;
    return true;
}

static bool chi_ring_pop(RingBuffer *rb, uint8_t *out_byte) {
    if (!rb || rb->size == 0) return false;
    if (out_byte) *out_byte = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->size--;
    return true;
}

static void chi_ring_free(RingBuffer *rb) {
    if (!rb) return;
    if (rb->buffer) free(rb->buffer);
    free(rb);
}

static void chi_init(void) {
    printf("Chi circular ring buffer module initialized v%s\n", CHI_VERSION);
}
