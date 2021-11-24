// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include <SDL2/SDL.h>
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

uint8_t keycode = 0;          // value of the pressed key
static int controller_axis_released = 1; // is analog axis released
input_msg_s key = {normal, 0};

uint8_t toggle_input_keyjazz() {
  keyjazz_enabled = !keyjazz_enabled;
  return keyjazz_enabled;
}

// Opens available game controllers and returns the amount of opened controllers
int initialize_game_controllers() {

  int num_joysticks = SDL_NumJoysticks();
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
  } else if (event->key.keysym.scancode == conf->key_gl_shader) {
    key = (input_msg_s){special, msg_toggle_gl_shader};
  } else if (event->key.keysym.scancode == conf->key_visualizer) {
    key = (input_msg_s){special, msg_toggle_special_fx};
  } else {
    key.value = 0;
  }
  return key;
}

static input_msg_s handle_game_controller_buttons(SDL_Event *event,
                                                  config_params_s *conf,
                                                  uint8_t keyvalue) {
  input_msg_s key = {normal, keyvalue};

  if (event->cbutton.button == conf->gamepad_up) {
    key.value = key_up;
  } else if (event->cbutton.button == conf->gamepad_left) {
    key.value = key_left;
  } else if (event->cbutton.button == conf->gamepad_down) {
    key.value = key_down;
  } else if (event->cbutton.button == conf->gamepad_right) {
    key.value = key_right;
  } else if (event->cbutton.button == conf->gamepad_select) {
    key.value = key_select;
  } else if (event->cbutton.button == conf->gamepad_start) {
    key.value = key_start;
  } else if (event->cbutton.button == conf->gamepad_opt) {
    key.value = key_opt;
  } else if (event->cbutton.button == conf->gamepad_edit) {
    key.value = key_edit;
  } else {
    key.value = 0;
  }
  return key;
}

static input_msg_s handle_game_controller_axis(SDL_Event *event,
                                               config_params_s *conf,
                                               uint8_t keyvalue) {

  input_msg_s key = {normal, keyvalue};

  // Handle up-down movement
  if (event->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY ||
      event->caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY) {

    if (event->caxis.value > 0) {
      key.value = key_down;
      if (event->caxis.value < conf->gamepad_analog_threshold) {
        if (controller_axis_released == 0) {
          controller_axis_released = 1;
          SDL_LogDebug(SDL_LOG_CATEGORY_INPUT, "Axis released");
        }
      } else {
        controller_axis_released = 0;
        SDL_LogDebug(SDL_LOG_CATEGORY_INPUT, "Axis down");
      }
    } else {
      key.value = key_up;
      if (event->caxis.value > 0 - conf->gamepad_analog_threshold) {
        if (controller_axis_released == 0) {
          controller_axis_released = 1;
          SDL_LogDebug(SDL_LOG_CATEGORY_INPUT, "Axis released");
        }
      } else {
        controller_axis_released = 0;
        SDL_LogDebug(SDL_LOG_CATEGORY_INPUT, "Axis up");
      }
    }
  }
  // Handle left-right movement
  else if (event->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
           event->caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX) {
    if (event->caxis.value > 0) {
      key.value = key_right;
      if (event->caxis.value < conf->gamepad_analog_threshold) {
        if (controller_axis_released == 0) {
          controller_axis_released = 1;
          SDL_LogDebug(SDL_LOG_CATEGORY_INPUT, "Axis released");
        }
      } else {
        controller_axis_released = 0;
        SDL_LogDebug(SDL_LOG_CATEGORY_INPUT, "Axis down");
      }
    } else {
      key.value = key_left;
      if (event->caxis.value > 0 - conf->gamepad_analog_threshold) {
        if (controller_axis_released == 0) {
          controller_axis_released = 1;
          SDL_LogDebug(SDL_LOG_CATEGORY_INPUT, "Axis released");
        }
      } else {
        controller_axis_released = 0;
        SDL_LogDebug(SDL_LOG_CATEGORY_INPUT, "Axis up");
      }
    }
  }

  return key;
}

// Handles SDL input events
void handle_sdl_events(config_params_s *conf) {

  SDL_Event event;

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

  // Game controller events
  case SDL_CONTROLLERBUTTONDOWN:
  case SDL_CONTROLLERBUTTONUP:
    key = handle_game_controller_buttons(&event, conf, 0);
    break;

  case SDL_CONTROLLERAXISMOTION:
    key = handle_game_controller_axis(&event, conf, 0);
    break;

  default:
    break;
  }

  // Do not allow pressing multiple keys with keyjazz
  if (key.type == normal) {

    if (event.type == SDL_KEYDOWN || event.type == SDL_CONTROLLERBUTTONDOWN ||
        controller_axis_released == 0) {
      keycode |= key.value;
    } else {
      keycode &= ~key.value;
    }

  } else {
    if (event.type == SDL_KEYDOWN)
      keycode = key.value;
    else
      keycode = 0;
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
