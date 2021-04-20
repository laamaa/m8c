#include <SDL2/SDL_events.h>
#include <SDL2/SDL_joystick.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_scancode.h>
#include <stdio.h>

#include "input.h"
#include "render.h"
#include "write.h"

SDL_Joystick *js;

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

uint8_t toggle_input_keyjazz() {
  keyjazz_enabled = !keyjazz_enabled;
  return keyjazz_enabled;
}

int initialize_input() {

  int num_joysticks = SDL_NumJoysticks();

  if (num_joysticks > 0) {
    js = SDL_JoystickOpen(0);
    SDL_JoystickEventState(SDL_ENABLE);
  }

  return num_joysticks;
}

void close_input() { SDL_JoystickClose(js); }

static input_msg_s handle_keyjazz(SDL_Event *event, uint8_t keyvalue) {
  input_msg_s key = {normal, keyvalue};
  switch (event->key.keysym.scancode) {
  case SDL_SCANCODE_Z:
    key.type = keyjazz;
    key.value = keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_S:
    key.type = keyjazz;
    key.value = 1 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_X:
    key.type = keyjazz;
    key.value = 2 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_D:
    key.type = keyjazz;
    key.value = 3 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_C:
    key.type = keyjazz;
    key.value = 4 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_V:
    key.type = keyjazz;
    key.value = 5 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_G:
    key.type = keyjazz;
    key.value = 6 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_B:
    key.type = keyjazz;
    key.value = 7 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_H:
    key.type = keyjazz;
    key.value = 8 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_N:
    key.type = keyjazz;
    key.value = 9 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_J:
    key.type = keyjazz;
    key.value = 10 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_M:
    key.type = keyjazz;
    key.value = 11 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_Q:
    key.type = keyjazz;
    key.value = 12 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_2:
    key.type = keyjazz;
    key.value = 13 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_W:
    key.type = keyjazz;
    key.value = 14 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_3:
    key.type = keyjazz;
    key.value = 15 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_E:
    key.type = keyjazz;
    key.value = 16 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_R:
    key.type = keyjazz;
    key.value = 17 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_5:
    key.type = keyjazz;
    key.value = 18 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_T:
    key.type = keyjazz;
    key.value = 19 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_6:
    key.type = keyjazz;
    key.value = 20 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_Y:
    key.type = keyjazz;
    key.value = 21 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_7:
    key.type = keyjazz;
    key.value = 22 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_U:
    key.type = keyjazz;
    key.value = 23 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_I:
    key.type = keyjazz;
    key.value = 24 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_9:
    key.type = keyjazz;
    key.value = 25 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_O:
    key.type = keyjazz;
    key.value = 26 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_0:
    key.type = keyjazz;
    key.value = 27 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_P:
    key.type = keyjazz;
    key.value = 28 + keyjazz_base_octave * 12;
    break;
  case SDL_SCANCODE_KP_DIVIDE:
    if (event->type == SDL_KEYDOWN && keyjazz_base_octave > 0) {
      keyjazz_base_octave--;
      display_keyjazz_overlay(1, keyjazz_base_octave);
    }
    break;
  case SDL_SCANCODE_KP_MULTIPLY:
    if (event->type == SDL_KEYDOWN && keyjazz_base_octave < 8) {
      keyjazz_base_octave++;
      display_keyjazz_overlay(1, keyjazz_base_octave);
    }
    break;
  default:
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

input_msg_s get_input_msg() {

  SDL_Event event;
  static uint8_t keycode = 0;
  input_msg_s key = {normal, 0};

  SDL_PollEvent(&event);

  switch (event.type) {

  // Handle SDL quit events (for example, X11 window close)
  case SDL_QUIT:
    return (input_msg_s){special, msg_quit};

  // Joystick axis movement events (directional buttons)
  case SDL_JOYAXISMOTION:

    switch (event.jaxis.axis) {

    case 0:

      if (event.jaxis.value < 0) {
        keycode |= key_left;
        break;
      }
      if (event.jaxis.value > 0) {
        keycode |= key_right;
        break;
      }
      if (event.jaxis.value == 0) {
        keycode &= ~key_left;
        keycode &= ~key_right;
        break;
      }

    case 1:

      if (event.jaxis.value < 0) {
        keycode |= key_up;
        break;
      }

      if (event.jaxis.value > 0) {
        keycode |= key_down;
        break;
      }

      if (event.jaxis.value == 0) {
        keycode &= ~key_up;
        keycode &= ~key_down;
        break;
      }
    }

    break;

  case SDL_JOYBUTTONDOWN:
  case SDL_JOYBUTTONUP:

    switch (event.jbutton.button) {

    case 1:
      key.type = normal;
      key.value = key_edit;
      break;
    case 2:
      key.type = normal;
      key.value = key_opt;
      break;
    case 8:
      key.type = normal;
      key.value = key_select;
      break;
    case 9:
      key.type = normal;
      key.value = key_start;
      break;
    }

    if (event.type == SDL_JOYBUTTONDOWN)
      keycode |= key.value;
    else
      keycode &= ~key.value;

    return (input_msg_s){key.type, keycode};

    break;

  // Special keyboard events
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
      return (input_msg_s){special, msg_quit};
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

  default:
    break;
  }

  // do not allow pressing multiple keys with keyjazz
  if (key.type == normal) {
    if (event.type == SDL_KEYDOWN)
      keycode |= key.value;
    else
      keycode &= ~key.value;
  } else {
    if (event.type == SDL_KEYDOWN)
      keycode = key.value;
    else
      keycode = 0;
  }

  return (input_msg_s){key.type, keycode};
}