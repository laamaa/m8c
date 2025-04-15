//
// Created by Jonne Kokkonen on 15.4.2025.
//

#ifndef INPUT_H
#define INPUT_H
#include "common.h"
#include "config.h"

#include <SDL3/SDL_events.h>

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
  unsigned char value;
  unsigned char value2;
} input_msg_s;

input_msg_s input_get_msg(config_params_s *conf);
int input_process_and_send(struct app_context *ctx);
void input_handle_key_down_event(struct app_context *ctx, const SDL_Event *event);
void input_handle_key_up_event(struct app_context *ctx, const SDL_Event *event);

#endif // INPUT_H
