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

/**
 * Initializes the message queue structure.
 *
 * @param queue A pointer to the message queue structure to be initialized.
 */
void init_queue(message_queue_s *queue);

/**
 * Retrieves and removes a message from the front of the message queue.
 * If the queue is empty, the function returns NULL.
 *
 * @param queue A pointer to the message queue structure from which the message is to be retrieved.
 * @param length A pointer to a variable where the length of the retrieved message will be stored.
 * @return A pointer to the retrieved message, or NULL if the queue is empty.
 */
unsigned char *pop_message(message_queue_s *queue, size_t *length);

/**
 * Adds a new message to the message queue.
 * If the queue is full, the message will not be added.
 *
 * @param queue A pointer to the message queue structure where the message is to be stored.
 * @param message A pointer to the message data to be added to the queue.
 * @param length The length of the message in bytes.
 */
void push_message(message_queue_s *queue, const unsigned char *message, size_t length);

/**
 * Calculates the current size of the message queue.
 *
 * @param queue A pointer to the message queue structure whose size is to be determined.
 * @return The number of messages currently in the queue.
 */
unsigned int queue_size(const message_queue_s *queue);

#endif // QUEUE_H
