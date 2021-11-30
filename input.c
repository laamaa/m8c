// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_gamecontroller.h>
#include <SDL2/SDL_joystick.h>
#include <SDL2/SDL_stdinc.h>
#include <stdio.h>

#include "config.h"
#include "input.h"
#include "render.h"
#include "write.h"

#define MAX_CONTROLLERS 4

SDL_GameController *game_controllers[MAX_CONTROLLERS];

// Bits for M8 input messages
enum keycodes {
  key_left = 1 << 7,
  key_up = 1 << 6,
  key_down = 1 << 5,
  key_select = 1 << 4,
  key_start = 1 << 3,
  key_right = 1 << 2,
  key_opt = 1 << 1,
  key_edit = 1
};

uint8_t keyjazz_enabled = 0;
uint8_t keyjazz_base_octave = 2;

static uint8_t keycode = 0; // value of the pressed key
static int num_joysticks = 0;

input_msg_s key = {normal, 0};

uint8_t toggle_input_keyjazz() {
  keyjazz_enabled = !keyjazz_enabled;
  return keyjazz_enabled;
}

// Opens available game controllers and returns the amount of opened controllers
int initialize_game_controllers() {

  num_joysticks = SDL_NumJoysticks();
  int controller_index = 0;

  SDL_Log("Looking for game controllers\n");
  SDL_Delay(
      1); // Some controllers like XBone wired need a little while to get ready
  // Open all available game controllers
  for (int i = 0; i < num_joysticks; i++) {
    if (!SDL_IsGameController(i))
      continue;
    if (controller_index >= MAX_CONTROLLERS)
      break;
    game_controllers[controller_index] = SDL_GameControllerOpen(i);
    SDL_Log("Controller %d: %s", controller_index + 1,
            SDL_GameControllerName(game_controllers[controller_index]));
    controller_index++;
  }

  // Read controller mapping database
  SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");

  return controller_index;
}

// Closes all open game controllers
void close_game_controllers() {

  for (int i = 0; i < MAX_CONTROLLERS; i++) {
    if (game_controllers[i])
      SDL_GameControllerClose(game_controllers[i]);
  }
}

static input_msg_s handle_keyjazz(SDL_Event *event, uint8_t keyvalue) {
  input_msg_s key = {keyjazz, keyvalue};
  switch (event->key.keysym.scancode) {
  case SDL_SCANCODE_Z:
    key.value = keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_S:
    key.value = 1 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_X:
    key.value = 2 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_D:
    key.value = 3 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_C:
    key.value = 4 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_V:
    key.value = 5 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_G:
    key.value = 6 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_B:
    key.value = 7 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_H:
    key.value = 8 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_N:
    key.value = 9 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_J:
    key.value = 10 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_M:
    key.value = 11 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_Q:
    key.value = 12 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_2:
    key.value = 13 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_W:
    key.value = 14 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_3:
    key.value = 15 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_E:
    key.value = 16 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_R:
    key.value = 17 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_5:
    key.value = 18 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_T:
    key.value = 19 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_6:
    key.value = 20 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_Y:
    key.value = 21 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_7:
    key.value = 22 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_U:
    key.value = 23 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_I:
    key.value = 24 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_9:
    key.value = 25 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_O:
    key.value = 26 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_0:
    key.value = 27 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_P:
    key.value = 28 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_KP_DIVIDE:
    key.type = normal;
    if (event->type == SDL_KEYDOWN && keyjazz_base_octave > 0) {
      keyjazz_base_octave--;
      display_keyjazz_overlay(1, keyjazz_base_octave);
    }
    break;
  case SDL_SCANCODE_KP_MULTIPLY:
    key.type = normal;
    if (event->type == SDL_KEYDOWN && keyjazz_base_octave < 8) {
      keyjazz_base_octave++;
      display_keyjazz_overlay(1, keyjazz_base_octave);
    }
    break;
  default:
    key.type = normal;
    break;
  }

  return key;
}

static input_msg_s handle_normal_keys(SDL_Event *event, config_params_s *conf,
                                      uint8_t keyvalue) {
  input_msg_s key = {normal, keyvalue};

  if (event->key.keysym.scancode == conf->key_up) {
    key.value = key_up;
  } else if (event->key.keysym.scancode == conf->key_left) {
    key.value = key_left;
  } else if (event->key.keysym.scancode == conf->key_down) {
    key.value = key_down;
  } else if (event->key.keysym.scancode == conf->key_right) {
    key.value = key_right;
  } else if (event->key.keysym.scancode == conf->key_select ||
             event->key.keysym.scancode == conf->key_select_alt) {
    key.value = key_select;
  } else if (event->key.keysym.scancode == conf->key_start ||
             event->key.keysym.scancode == conf->key_start_alt) {
    key.value = key_start;
  } else if (event->key.keysym.scancode == conf->key_opt ||
             event->key.keysym.scancode == conf->key_opt_alt) {
    key.value = key_opt;
  } else if (event->key.keysym.scancode == conf->key_edit ||
             event->key.keysym.scancode == conf->key_edit_alt) {
    key.value = key_edit;
  } else if (event->key.keysym.scancode == conf->key_delete) {
    key.value = key_opt | key_edit;
  } else if (event->key.keysym.scancode == conf->key_reset) {
    key = (input_msg_s){special, msg_reset_display};
  } else {
    key.value = 0;
  }
  return key;
}

int axis_in_threshold(int axis_value, int threshold) {
  if (axis_value <= 0 - threshold || axis_value >= threshold) {
    return 1;
  } else {
    return 0;
  }
}

static int get_game_controller_button(config_params_s *conf,
                                      SDL_GameController *controller,
                                      int button) {

  const int button_mappings[8] = {conf->gamepad_up,     conf->gamepad_down,
                                  conf->gamepad_left,   conf->gamepad_right,
                                  conf->gamepad_opt,    conf->gamepad_edit,
                                  conf->gamepad_select, conf->gamepad_start};

  if (SDL_GameControllerGetButton(controller, button_mappings[button])) {
    return 1;
  } else {
    switch (button) {
    case INPUT_UP:
      return SDL_GameControllerGetAxis(controller,
                                       conf->gamepad_analog_axis_updown) <
             -conf->gamepad_analog_threshold;
    case INPUT_DOWN:
      return SDL_GameControllerGetAxis(controller,
                                       conf->gamepad_analog_axis_updown) >
             conf->gamepad_analog_threshold;
    case INPUT_LEFT:
      return SDL_GameControllerGetAxis(controller,
                                       conf->gamepad_analog_axis_leftright) <
             -conf->gamepad_analog_threshold;
    case INPUT_RIGHT:
      return SDL_GameControllerGetAxis(controller,
                                       conf->gamepad_analog_axis_leftright) >
             conf->gamepad_analog_threshold;
    case INPUT_OPT:
      return SDL_GameControllerGetAxis(controller,
                                       conf->gamepad_analog_axis_opt) >
             conf->gamepad_analog_threshold;
    case INPUT_EDIT:
      return SDL_GameControllerGetAxis(controller,
                                       conf->gamepad_analog_axis_edit) >
             conf->gamepad_analog_threshold;
    case INPUT_SELECT:
      return SDL_GameControllerGetAxis(controller,
                                       conf->gamepad_analog_axis_select) >
             conf->gamepad_analog_threshold;
    case INPUT_START:
      return SDL_GameControllerGetAxis(controller,
                                       conf->gamepad_analog_axis_start) >
             conf->gamepad_analog_threshold;
    default:
      return 0;
    }
  }
  return 0;
}

// Handle game controllers, simply check all buttons and analog axis on every
// cycle
static int handle_game_controller_buttons(config_params_s *conf) {

  const int keycodes[8] = {key_up,  key_down, key_left,   key_right,
                           key_opt, key_edit, key_select, key_start};

  int key = 0;

  // Cycle through every active game controller
  for (int gc = 0; gc < num_joysticks; gc++) {
    // Cycle through all M8 buttons
    for (int button = 0; button < (input_buttons_t)INPUT_MAX; button++) {
      // If the button is active, add the keycode to the variable containing
      // active keys
      if (get_game_controller_button(conf, game_controllers[gc], button)) {
        key |= keycodes[button];
      }
    }
  }

  return key;
}

// Handles SDL input events
void handle_sdl_events(config_params_s *conf) {

  static int prev_key_analog = 0;

  SDL_Event event;

  // Read joysticks
  int key_analog = handle_game_controller_buttons(conf);
  if (prev_key_analog != key_analog) {
    keycode = key_analog;
    prev_key_analog = key_analog;
  }

  SDL_PollEvent(&event);

  switch (event.type) {

  // Reinitialize game controllers on controller add/remove/remap
  case SDL_CONTROLLERDEVICEADDED:
  case SDL_CONTROLLERDEVICEREMOVED:
    initialize_game_controllers();
    break;

  // Handle SDL quit events (for example, window close)
  case SDL_QUIT:
    key = (input_msg_s){special, msg_quit};
    break;

  // Keyboard events. Special events are handled within SDL_KEYDOWN.
  case SDL_KEYDOWN:

    // ALT+ENTER toggles fullscreen
    if (event.key.keysym.sym == SDLK_RETURN &&
        (event.key.keysym.mod & KMOD_ALT) > 0) {
      toggle_fullscreen();
      break;
    }

    // ALT+F4 quits program
    if (event.key.keysym.sym == SDLK_F4 &&
        (event.key.keysym.mod & KMOD_ALT) > 0) {
      key = (input_msg_s){special, msg_quit};
    }

    // ESC = toggle keyjazz
    if (event.key.keysym.sym == SDLK_ESCAPE) {
      display_keyjazz_overlay(toggle_input_keyjazz(), keyjazz_base_octave);
    }

  // Normal keyboard inputs
  case SDL_KEYUP:
    key = handle_normal_keys(&event, conf, 0);

    if (keyjazz_enabled)
      key = handle_keyjazz(&event, key.value);
    break;

  default:
    break;
  }

  switch (key.type) {
  case normal:
    if (event.type == SDL_KEYDOWN) {
      keycode |= key.value;
    } else {
      keycode &= ~key.value;
    }
    break;
  case keyjazz:
    // Do not allow pressing multiple keys with keyjazz
  case special:
    if (event.type == SDL_KEYDOWN) {
      keycode = key.value;
    } else {
      keycode = 0;
    }
    break;
  default:
    break;
  }
}

// Returns the currently pressed keys to main
input_msg_s get_input_msg(config_params_s *conf) {

  key = (input_msg_s){normal, 0};

  // Query for SDL events
  handle_sdl_events(conf);

  if (keycode == (key_start | key_select | key_opt | key_edit)) {
    key = (input_msg_s){special, msg_reset_display};
  }

  if (key.type == normal) {
    /* Normal input keys go through some event-based manipulation in
       handle_sdl_events(), the value is stored in keycode variable */
    return (input_msg_s){key.type, keycode};
  } else {
    // Special event keys already have the correct keycode baked in
    return key;
  }
}