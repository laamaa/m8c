// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_gamecontroller.h>
#include <stdio.h>

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

uint8_t keycode = 0; // value of the pressed key
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
  // Open all available game controllers
  for (int i = 0; i < num_joysticks; i++) {
    if (!SDL_IsGameController(i))
      continue;
    if (controller_index >= MAX_CONTROLLERS)
      break;
    game_controllers[controller_index] = SDL_GameControllerOpen(i);
    SDL_Log("Controller %d: %s",controller_index+1,SDL_GameControllerName(game_controllers[controller_index]));
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

static input_msg_s handle_normal_keys(SDL_Event *event, uint8_t keyvalue) {
  input_msg_s key = {normal, keyvalue};
  switch (event->key.keysym.scancode) {

  case SDL_SCANCODE_UP:
    key.value = key_up;
    break;

  case SDL_SCANCODE_LEFT:
    key.value = key_left;
    break;

  case SDL_SCANCODE_DOWN:
    key.value = key_down;
    break;

  case SDL_SCANCODE_RIGHT:
    key.value = key_right;
    break;

  case SDL_SCANCODE_LSHIFT:
  case SDL_SCANCODE_A:
    key.value = key_select;
    break;

  case SDL_SCANCODE_SPACE:
  case SDL_SCANCODE_S:
    key.value = key_start;
    break;

  case SDL_SCANCODE_LALT:
  case SDL_SCANCODE_Z:
    key.value = key_opt;
    break;

  case SDL_SCANCODE_LCTRL:
  case SDL_SCANCODE_X:
    key.value = key_edit;
    break;

  case SDL_SCANCODE_DELETE:
    key.value = key_opt | key_edit;
    break;

  default:
    key.value = 0;
    break;
  }
  return key;
}

static input_msg_s handle_game_controller_buttons(SDL_Event *event,
                                                  uint8_t keyvalue) {
  input_msg_s key = {normal, keyvalue};
  switch (event->cbutton.button) {

  case SDL_CONTROLLER_BUTTON_DPAD_UP:
    key.value = key_up;
    break;

  case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
    key.value = key_down;
    break;

  case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
    key.value = key_left;
    break;

  case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
    key.value = key_right;
    break;

  case SDL_CONTROLLER_BUTTON_BACK:
    key.value = key_select;
    break;

  case SDL_CONTROLLER_BUTTON_START:
    key.value = key_start;
    break;

  case SDL_CONTROLLER_BUTTON_B:
    key.value = key_edit;
    break;

  case SDL_CONTROLLER_BUTTON_A:
    key.value = key_opt;
    break;

  default:
    key.value = 0;
    break;
  }

  return key;
}

// Handles SDL input events
void handle_sdl_events() {

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
    key = handle_normal_keys(&event, 0);

    if (keyjazz_enabled)
      key = handle_keyjazz(&event, key.value);
    break;

  // Game controller events
  case SDL_CONTROLLERBUTTONDOWN:
  case SDL_CONTROLLERBUTTONUP:
    key = handle_game_controller_buttons(&event, 0);
    break;

  default:
    break;
  }

  // Do not allow pressing multiple keys with keyjazz
  if (key.type == normal) {
    if (event.type == SDL_KEYDOWN || event.type == SDL_CONTROLLERBUTTONDOWN)
      keycode |= key.value;
    else
      keycode &= ~key.value;
  } else {
    if (event.type == SDL_KEYDOWN)
      keycode = key.value;
    else
      keycode = 0;
  }
}

// Returns the currently pressed keys to main
input_msg_s get_input_msg() {

  key = (input_msg_s){normal, 0};

  // Query for SDL events
  handle_sdl_events();

  return (input_msg_s){key.type, keycode};
}