// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef INPUT_H_
#define INPUT_H_

#include <stdint.h>
#include "config.h"

typedef enum input_buttons_t {
    INPUT_UP,
    INPUT_DOWN,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_OPT,
    INPUT_EDIT,
    INPUT_SELECT,
    INPUT_START,
    INPUT_MAX
} input_buttons_t;

typedef enum input_type_t {
  normal,
  keyjazz,
  special
} input_type_t;

typedef enum special_messages_t {
  msg_quit = 1,
  msg_reset_display = 2
} special_messages_t;

typedef struct input_msg_s {
  input_type_t type;
  uint8_t value;
  uint8_t value2;
  uint32_t eventType;
} input_msg_s;

int initialize_game_controllers();
void close_game_controllers();
input_msg_s get_input_msg(config_params_s *conf);

#endif
