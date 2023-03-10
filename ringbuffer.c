#include "ringbuffer.h"
#include <SDL.h>

RingBuffer *ring_buffer_create(uint32_t size) {
  RingBuffer *rb = SDL_malloc(sizeof(*rb));
  rb->buffer = SDL_malloc(sizeof(*(rb->buffer)) * size);
  rb->head = 0;
  rb->tail = 0;
  rb->max_size = size;
  rb->size = 0;
  return rb;
}

void ring_buffer_free(RingBuffer *rb) {
  free(rb->buffer);
  free(rb);
}

uint32_t ring_buffer_empty(RingBuffer *rb) {
  return (rb->size == 0);
}

uint32_t ring_buffer_full(RingBuffer *rb) {
  return (rb->size == rb->max_size);
}

uint32_t ring_buffer_push(RingBuffer *rb, const uint8_t *data, uint32_t length) {
  if (ring_buffer_full(rb)) {
    return -1; // buffer full, push fails
  } else {
    uint32_t space1 = rb->max_size - rb->tail;
    uint32_t n = (length <= rb->max_size - rb->size) ? length : (rb->max_size - rb->size);
    if (n <= space1) {
      SDL_memcpy(rb->buffer + rb->tail, data, n);
    } else {
      SDL_memcpy(rb->buffer + rb->tail, data, space1);
      SDL_memcpy(rb->buffer, data + space1, n - space1);
    }
    rb->tail = (rb->tail + n) % rb->max_size;
    rb->size += n;
    return n; // push successful, returns number of bytes pushed
  }
}

uint32_t ring_buffer_pop(RingBuffer *rb, uint8_t *data, uint32_t length) {
  if (ring_buffer_empty(rb)) {
    return -1; // buffer empty, pop fails
  } else {
    uint32_t space1 = rb->max_size - rb->head;
    uint32_t n = (length <= rb->size) ? length : rb->size;
    if (n <= space1) {
      SDL_memcpy(data, rb->buffer + rb->head, n);
    } else {
      SDL_memcpy(data, rb->buffer + rb->head, space1);
      SDL_memcpy(data + space1, rb->buffer, n - space1);
    }
    rb->head = (rb->head + n) % rb->max_size;
    rb->size -= n;
    return n; // pop successful, returns number of bytes popped
  }
}
