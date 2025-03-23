// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef INPUT_H_
#define INPUT_H_

#include "config.h"
#include <stdint.h>

enum app_state { QUIT, WAIT_FOR_DEVICE, RUN };

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

// Bits for M8 input messages
typedef enum keycodes_t {
  key_left = 1 << 7,
  key_up = 1 << 6,
  key_down = 1 << 5,
  key_select = 1 << 4,
  key_start = 1 << 3,
  key_right = 1 << 2,
  key_opt = 1 << 1,
  key_edit = 1
} keycodes_t;

typedef enum input_type_t { normal, keyjazz, special } input_type_t;

typedef enum special_messages_t {
  msg_quit = 1,
  msg_reset_display = 2,
  msg_toggle_audio = 3
} special_messages_t;

typedef struct input_msg_s {
  input_type_t type;
  uint8_t value;
  uint8_t value2;
  uint32_t eventType;
} input_msg_s;

input_msg_s input_get_msg(config_params_s *conf);
int input_process(config_params_s conf, enum app_state *app_state);

#endif
