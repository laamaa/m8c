#ifndef INPUT_H_
#define INPUT_H_

#include <stdint.h>

typedef enum input_type_t {
  normal,
  keyjazz,
  special
} input_type_t;

typedef enum special_messages_t {
  msg_quit
} special_messages_t;

typedef struct input_msg_s {
  input_type_t type;
  uint8_t value;
} input_msg_s;

int initialize_input();
void close_input();
input_msg_s get_input_msg();

#endif