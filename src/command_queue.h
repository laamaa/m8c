#ifndef COMMAND_QUEUE_H_
#define COMMAND_QUEUE_H_
#include <stdint.h>

void command_queue_init();
int command_queue_push(uint8_t *data, uint32_t size);
int command_queue_pop(uint8_t **data, uint32_t *size);
void command_queue_destroy();
#endif
