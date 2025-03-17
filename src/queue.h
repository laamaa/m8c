//
// Created by jonne on 3/17/25.
//

#ifndef QUEUE_H
#define QUEUE_H

#include <SDL3/SDL.h>

#define MAX_QUEUE_SIZE 4096

typedef struct {
  unsigned char *messages[MAX_QUEUE_SIZE];
  size_t lengths[MAX_QUEUE_SIZE]; // Store lengths of each message
  int front;
  int rear;
  SDL_Mutex *mutex;
  SDL_Condition *cond;
} message_queue_s;

// Initialize the message queue
void init_queue(message_queue_s *queue);
unsigned char *pop_message(message_queue_s *queue, size_t *length);
void push_message(message_queue_s *queue, const unsigned char *message, size_t length);

#endif // QUEUE_H
