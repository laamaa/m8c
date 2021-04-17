#include <SDL2/SDL_events.h>
#include <SDL2/SDL_joystick.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_scancode.h>
#include <stdio.h>

#include "input.h"
#include "render.h"

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

int initialize_input() {
  int num_joysticks = SDL_NumJoysticks();
  if (num_joysticks > 0) {
    js = SDL_JoystickOpen(0);
    SDL_JoystickEventState(SDL_ENABLE);
    fprintf(stderr, "Opened Joystick 0");
  }
  return num_joysticks;
}

void close_input() { SDL_JoystickClose(js); }

uint8_t process_input() {
  SDL_Event event;
  uint8_t key = 0;
  static uint8_t input = 0;

  SDL_PollEvent(&event);
  switch (event.type) {

  case SDL_QUIT:
    return 21;

  case SDL_JOYAXISMOTION:

    switch (event.jaxis.axis) {
    case 0:
      if (event.jaxis.value < 0) {
        input |= key_left;
        break;
      }
      if (event.jaxis.value > 0) {
        input |= key_right;
        break;
      }
      if (event.jaxis.value == 0) {
        input &= ~key_left;
        input &= ~key_right;
        break;
      }
    case 1:
      if (event.jaxis.value < 0) {
        input |= key_up;
        break;
      }
      if (event.jaxis.value > 0) {
        input |= key_down;
        break;
      }
      if (event.jaxis.value == 0) {
        input &= ~key_up;
        input &= ~key_down;
        break;
      }
    }
    break;

  case SDL_JOYBUTTONDOWN:
  case SDL_JOYBUTTONUP:
    switch (event.jbutton.button) {
    case 1:
      key = key_edit;
      break;
    case 2:
      key = key_opt;
      break;
    case 8:
      key = key_select;
      break;
    case 9:
      key = key_start;
      break;
    }

    if (event.type == SDL_JOYBUTTONDOWN) {
      input |= key;
    } else {
      input &= ~key;
    }
    return input;
    break;

  case SDL_KEYDOWN:

    //ALT+ENTER toggles fullscreen
    if (event.key.keysym.sym == SDLK_RETURN &&
        (event.key.keysym.mod & KMOD_ALT) > 0) {
      toggle_fullscreen();
    }

    //CTRL+q quits program
    if (event.key.keysym.sym == SDLK_q && (event.key.keysym.mod & KMOD_CTRL) > 0){
      return 21; //arbitary quit code for main loop
    }

  case SDL_KEYUP:

    switch (event.key.keysym.scancode) {
    case SDL_SCANCODE_I:
    case SDL_SCANCODE_UP:
      key = key_up;
      break;
    case SDL_SCANCODE_J:
    case SDL_SCANCODE_LEFT:
      key = key_left;
      break;
    case SDL_SCANCODE_K:
    case SDL_SCANCODE_DOWN:
      key = key_down;
      break;
    case SDL_SCANCODE_L:
    case SDL_SCANCODE_RIGHT:
      key = key_right;
      break;
    case SDL_SCANCODE_A:
      key = key_select;
      break;
    case SDL_SCANCODE_S:
      key = key_start;
      break;
    case SDL_SCANCODE_Z:
      key = key_opt;
      break;
    case SDL_SCANCODE_X:
      key = key_edit;
      break;
    default:
      key = 0;
      break;
    }
    break;
  default:
    break;
  }

  if (event.type == SDL_KEYDOWN) {
    input |= key;
  } else {
    input &= ~key;
  }

  return input;
}