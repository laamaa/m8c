#ifndef M8C_RINGBUFFER_H
#define M8C_RINGBUFFER_H

#include <stdint.h>

typedef struct {
    uint8_t *buffer;
    uint32_t head;
    uint32_t tail;
    uint32_t max_size;
    uint32_t size;
} RingBuffer;

RingBuffer *ring_buffer_create(uint32_t size);

uint32_t ring_buffer_empty(RingBuffer *rb);

uint32_t ring_buffer_pop(RingBuffer *rb, uint8_t *data, uint32_t length);

uint32_t ring_buffer_push(RingBuffer *rb, const uint8_t *data, uint32_t length);

void ring_buffer_free(RingBuffer *rb);

#endif //M8C_RINGBUFFER_H
