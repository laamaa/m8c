#include "queue.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

// Initialize the message queue
void init_queue(message_queue_s *queue) {
    queue->front = 0;
    queue->rear = 0;
    queue->mutex = SDL_CreateMutex();
    queue->cond = SDL_CreateCondition();
}

// Free allocated memory and destroy mutex
void destroy_queue(message_queue_s *queue) {
  SDL_LockMutex(queue->mutex);

  while (queue->front != queue->rear) {
    SDL_free(queue->messages[queue->front]);
    queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
  }

  SDL_UnlockMutex(queue->mutex);
  SDL_DestroyMutex(queue->mutex);
  SDL_DestroyCondition(queue->cond);
}

// Push a message to the queue
void push_message(message_queue_s *queue, const unsigned char *message, size_t length) {
    SDL_LockMutex(queue->mutex);

    if ((queue->rear + 1) % MAX_QUEUE_SIZE == queue->front) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM,"Queue is full, cannot add message.");
    } else {
        // Allocate space for the message and store it
        queue->messages[queue->rear] = SDL_malloc(length);
        SDL_memcpy(queue->messages[queue->rear], message, length);
        queue->lengths[queue->rear] = length;
        queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
        SDL_SignalCondition(queue->cond);  // Signal consumer thread
    }

    SDL_UnlockMutex(queue->mutex);
}

// Pop a message from the queue
unsigned char *pop_message(message_queue_s *queue, size_t *length) {
  SDL_LockMutex(queue->mutex);

  // Check if the queue is empty
  if (queue->front == queue->rear) {
    SDL_UnlockMutex(queue->mutex);
    return NULL;  // Return NULL if there are no messages
  }

  // Otherwise, retrieve the message and its length
  *length = queue->lengths[queue->front];
  unsigned char *message = queue->messages[queue->front];
  queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;

  SDL_UnlockMutex(queue->mutex);
  return message;
}

unsigned int queue_size(const message_queue_s *queue) {
  SDL_LockMutex(queue->mutex);
  const unsigned int size = (queue->rear - queue->front + MAX_QUEUE_SIZE) % MAX_QUEUE_SIZE;
  SDL_UnlockMutex(queue->mutex);
  return size;
}