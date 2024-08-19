// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include <SDL.h>
#include <stdio.h>

#include "config.h"
#include "input.h"
#include "render.h"
#include "gamecontrollers.h"

uint8_t keyjazz_enabled = 0;
uint8_t keyjazz_base_octave = 2;
uint8_t keyjazz_velocity = 0x64;

static uint8_t keycode = 0; // value of the pressed key

static input_msg_s key = {normal, 0};

uint8_t toggle_input_keyjazz() {
  keyjazz_enabled = !keyjazz_enabled;
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, keyjazz_enabled ? "Keyjazz enabled" : "Keyjazz disabled");
  return keyjazz_enabled;
}

static input_msg_s handle_keyjazz(SDL_Event *event, uint8_t keyvalue, config_params_s *conf) {
  input_msg_s key = {keyjazz, keyvalue, keyjazz_velocity, event->type};
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
  default:
    key.type = normal;
    if (event->key.repeat > 0 || event->key.type == SDL_KEYUP) {
      break;
    }
    if (event->key.keysym.scancode == conf->key_jazz_dec_octave) {
      if (keyjazz_base_octave > 0) {
        keyjazz_base_octave--;
        display_keyjazz_overlay(1, keyjazz_base_octave, keyjazz_velocity);
      }
    } else if (event->key.keysym.scancode == conf->key_jazz_inc_octave) {
      if (keyjazz_base_octave < 8) {
        keyjazz_base_octave++;
        display_keyjazz_overlay(1, keyjazz_base_octave, keyjazz_velocity);
      }
    } else if (event->key.keysym.scancode == conf->key_jazz_dec_velocity) {
      if ((event->key.keysym.mod & KMOD_ALT) > 0) {
        if (keyjazz_velocity > 1)
          keyjazz_velocity -= 1;
      } else {
        if (keyjazz_velocity > 0x10)
          keyjazz_velocity -= 0x10;
      }
      display_keyjazz_overlay(1, keyjazz_base_octave, keyjazz_velocity);
    } else if (event->key.keysym.scancode == conf->key_jazz_inc_velocity) {
      if ((event->key.keysym.mod & KMOD_ALT) > 0) {
        if (keyjazz_velocity < 0x7F)
          keyjazz_velocity += 1;
      } else {
        if (keyjazz_velocity < 0x6F)
          keyjazz_velocity += 0x10;
      }
      display_keyjazz_overlay(1, keyjazz_base_octave, keyjazz_velocity);
    }
    break;
  }

  return key;
}

static input_msg_s handle_normal_keys(SDL_Event *event, config_params_s *conf, uint8_t keyvalue) {
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
  } else if (event->key.keysym.scancode == conf->key_toggle_audio) {
    key = (input_msg_s){special, msg_toggle_audio};

  } else {
    key.value = 0;
  }
  return key;
}

// Handles SDL input events
void handle_sdl_events(config_params_s *conf) {

  static int prev_key_analog = 0;

  SDL_Event event;

  // Read joysticks
  int key_analog = gamecontrollers_handle_buttons(conf);
  if (prev_key_analog != key_analog) {
    keycode = key_analog;
    prev_key_analog = key_analog;
  }

  input_msg_s gcmsg = gamecontrollers_handle_special_messages(conf);
  if (gcmsg.type == special) { key = gcmsg; }

  while (SDL_PollEvent(&event)) {

    switch (event.type) {

    // Reinitialize game controllers on controller add/remove/remap
    case SDL_CONTROLLERDEVICEADDED:
    case SDL_CONTROLLERDEVICEREMOVED:
      gamecontrollers_initialize();
      break;

    // Handle SDL quit events (for example, window close)
    case SDL_QUIT:
      key = (input_msg_s){special, msg_quit};
      break;

    case SDL_WINDOWEVENT:
      if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
        static uint32_t ticks_window_resized = 0;
        if (SDL_GetTicks() - ticks_window_resized > 500) {
          SDL_Log("Resizing window...");
          key = (input_msg_s){special, msg_reset_display};
          ticks_window_resized = SDL_GetTicks();
        }
      }
      break;

    // Keyboard events. Special events are handled within SDL_KEYDOWN.
    case SDL_KEYDOWN:

      if (event.key.repeat > 0) {
        break;
      }

      // ALT+ENTER toggles fullscreen
      if (event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT) > 0) {
        toggle_fullscreen();
        break;
      }

      // ALT+F4 quits program
      else if (event.key.keysym.sym == SDLK_F4 && (event.key.keysym.mod & KMOD_ALT) > 0) {
        key = (input_msg_s){special, msg_quit};
        break;
      }

      // ESC = toggle keyjazz
      else if (event.key.keysym.sym == SDLK_ESCAPE) {
        display_keyjazz_overlay(toggle_input_keyjazz(), keyjazz_base_octave, keyjazz_velocity);
      }

    // Normal keyboard inputs
    case SDL_KEYUP:
      key = handle_normal_keys(&event, conf, 0);

      if (keyjazz_enabled)
        key = handle_keyjazz(&event, key.value, conf);
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
}

// Returns the currently pressed keys to main
input_msg_s get_input_msg(config_params_s *conf) {

  key = (input_msg_s){normal, 0};

  // Query for SDL events
  handle_sdl_events(conf);

  if (!keyjazz_enabled && keycode == (key_start | key_select | key_opt | key_edit)) {
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
