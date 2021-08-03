// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef INPUT_H_
#define INPUT_H_

#include <stdint.h>

typedef enum input_type_t { normal, keyjazz, special } input_type_t;

typedef enum special_messages_t {
  msg_quit = 1,
  msg_reset_display = 2,
  msg_toggle_special_fx = 3
} special_messages_t;

typedef struct input_msg_s {
  input_type_t type;
  uint8_t value;
} input_msg_s;

void close_game_controllers();
input_msg_s get_input_msg();

#endif