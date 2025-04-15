#include "events.h"
#include "backends/audio.h"
#include "backends/m8.h"
#include "common.h"
#include "config.h"
#include "gamecontrollers.h"
#include "render.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>

uint8_t keyjazz_enabled = 0;
uint8_t keyjazz_base_octave = 2;
uint8_t keyjazz_velocity = 0x64;

static uint8_t keycode = 0; // value of the pressed key

static input_msg_s key = {normal, 0, 0, 0};

static unsigned char toggle_input_keyjazz() {
  keyjazz_enabled = !keyjazz_enabled;
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, keyjazz_enabled ? "Keyjazz enabled" : "Keyjazz disabled");
  return keyjazz_enabled;
}

// Get note value for a scancode, or -1 if not found
static int get_note_for_scancode(SDL_Scancode scancode) {

  // Map from SDL scancodes to note offsets
  const struct keyjazz_scancodes_t {
    SDL_Scancode scancode;
    uint8_t note_offset;
  } NOTE_MAP[] = {
      {SDL_SCANCODE_Z, 0},  {SDL_SCANCODE_S, 1},  {SDL_SCANCODE_X, 2},  {SDL_SCANCODE_D, 3},
      {SDL_SCANCODE_C, 4},  {SDL_SCANCODE_V, 5},  {SDL_SCANCODE_G, 6},  {SDL_SCANCODE_B, 7},
      {SDL_SCANCODE_H, 8},  {SDL_SCANCODE_N, 9},  {SDL_SCANCODE_J, 10}, {SDL_SCANCODE_M, 11},
      {SDL_SCANCODE_Q, 12}, {SDL_SCANCODE_2, 13}, {SDL_SCANCODE_W, 14}, {SDL_SCANCODE_3, 15},
      {SDL_SCANCODE_E, 16}, {SDL_SCANCODE_R, 17}, {SDL_SCANCODE_5, 18}, {SDL_SCANCODE_T, 19},
      {SDL_SCANCODE_6, 20}, {SDL_SCANCODE_Y, 21}, {SDL_SCANCODE_7, 22}, {SDL_SCANCODE_U, 23},
      {SDL_SCANCODE_I, 24}, {SDL_SCANCODE_9, 25}, {SDL_SCANCODE_O, 26}, {SDL_SCANCODE_0, 27},
      {SDL_SCANCODE_P, 28},
  };

  const size_t NOTE_MAP_SIZE = (sizeof(NOTE_MAP) / sizeof(NOTE_MAP[0]));

  for (size_t i = 0; i < NOTE_MAP_SIZE; i++) {
    if (NOTE_MAP[i].scancode == scancode) {
      return NOTE_MAP[i].note_offset + keyjazz_base_octave * 12;
    }
  }
  return -1; // Not a note key
}

// Handle octave and velocity changes
static void handle_keyjazz_settings(const SDL_Event *event, const config_params_s *conf) {

  // Constants for keyjazz limits and adjustments
  const unsigned char KEYJAZZ_MIN_OCTAVE = 0;
  const unsigned char KEYJAZZ_MAX_OCTAVE = 8;
  const unsigned char KEYJAZZ_MIN_VELOCITY = 0;
  const unsigned char KEYJAZZ_MAX_VELOCITY = 0x7F;
  const unsigned char KEYJAZZ_FINE_VELOCITY_STEP = 1;
  const unsigned char KEYJAZZ_COARSE_VELOCITY_STEP = 0x10;

  if (event->key.repeat > 0 || event->key.type == SDL_EVENT_KEY_UP) {
    return;
  }

  const SDL_Scancode scancode = event->key.scancode;
  const bool is_fine_adjustment = (event->key.mod & SDL_KMOD_ALT) > 0;

  if (scancode == conf->key_jazz_dec_octave && keyjazz_base_octave > KEYJAZZ_MIN_OCTAVE) {
    keyjazz_base_octave--;
    display_keyjazz_overlay(1, keyjazz_base_octave, keyjazz_velocity);
  } else if (scancode == conf->key_jazz_inc_octave && keyjazz_base_octave < KEYJAZZ_MAX_OCTAVE) {
    keyjazz_base_octave++;
    display_keyjazz_overlay(1, keyjazz_base_octave, keyjazz_velocity);
  } else if (scancode == conf->key_jazz_dec_velocity) {
    const int step = is_fine_adjustment ? KEYJAZZ_FINE_VELOCITY_STEP : KEYJAZZ_COARSE_VELOCITY_STEP;
    if (keyjazz_velocity > (is_fine_adjustment ? KEYJAZZ_MIN_VELOCITY + step : step)) {
      keyjazz_velocity -= step;
      display_keyjazz_overlay(1, keyjazz_base_octave, keyjazz_velocity);
    }
  } else if (scancode == conf->key_jazz_inc_velocity) {
    const int step = is_fine_adjustment ? KEYJAZZ_FINE_VELOCITY_STEP : KEYJAZZ_COARSE_VELOCITY_STEP;
    const int max = is_fine_adjustment ? KEYJAZZ_MAX_VELOCITY : (KEYJAZZ_MAX_VELOCITY - step);
    if (keyjazz_velocity < max) {
      keyjazz_velocity += step;
      display_keyjazz_overlay(1, keyjazz_base_octave, keyjazz_velocity);
    }
  }
}

static input_msg_s handle_keyjazz(SDL_Event *event, uint8_t keyvalue, config_params_s *conf) {
  input_msg_s key = {keyjazz, keyvalue, keyjazz_velocity, event->type};

  // Check if this is a note key
  const int note_value = get_note_for_scancode(event->key.scancode);
  if (note_value >= 0) {
    key.value = note_value;
    return key;
  }

  // Not a note key, handle other settings
  key.type = normal;
  handle_keyjazz_settings(event, conf);

  return key;
}

static input_msg_s handle_m8_buttons(const SDL_Event *event, const config_params_s *conf) {
  // Default message with normal type and no value
  input_msg_s key = {normal, 0, 0, 0};

  // Get the current scancode
  const SDL_Scancode scancode = event->key.scancode;

  // Handle standard keycodes (single key mapping)
  const struct {
    SDL_Scancode scancode;
    uint8_t value;
  } normal_key_map[] = {
      {conf->key_up, key_up},
      {conf->key_left, key_left},
      {conf->key_down, key_down},
      {conf->key_right, key_right},
      {conf->key_select, key_select},
      {conf->key_select_alt, key_select},
      {conf->key_start, key_start},
      {conf->key_start_alt, key_start},
      {conf->key_opt, key_opt},
      {conf->key_opt_alt, key_opt},
      {conf->key_edit, key_edit},
      {conf->key_edit_alt, key_edit},
      {conf->key_delete, key_opt | key_edit},
  };

  // Handle special messages (different message type)
  const struct {
    SDL_Scancode scancode;
    special_messages_t message;
  } special_key_map[] = {
      {conf->key_reset, msg_reset_display},
      {conf->key_toggle_audio, msg_toggle_audio},
  };

  // Check normal key mappings
  for (size_t i = 0; i < sizeof(normal_key_map) / sizeof(normal_key_map[0]); i++) {
    if (scancode == normal_key_map[i].scancode) {
      key.value = normal_key_map[i].value;
      return key;
    }
  }

  // Check special key mappings
  for (size_t i = 0; i < sizeof(special_key_map) / sizeof(special_key_map[0]); i++) {
    if (scancode == special_key_map[i].scancode) {
      key.type = special;
      key.value = special_key_map[i].message;
      return key;
    }
  }

  // No matching key found, return default key message
  return key;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  struct app_context *ctx = appstate;
  SDL_AppResult ret_val = SDL_APP_CONTINUE;
  static int prev_key_analog = 0;

  switch (event->type) {

  // --- System events ---
  case SDL_EVENT_QUIT:
  case SDL_EVENT_TERMINATING:
    ret_val = SDL_APP_SUCCESS;
    break;
  case SDL_EVENT_DID_ENTER_BACKGROUND:
    // iOS: Application entered into background on iOS. About 5 seconds to stop things.
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Received SDL_EVENT_DID_ENTER_BACKGROUND");
    ctx->app_suspended = 1;
    if (ctx->device_connected)
      m8_pause_processing();
    break;
  case SDL_EVENT_WILL_ENTER_BACKGROUND:
    // iOS: App about to enter into background
    break;
  case SDL_EVENT_WILL_ENTER_FOREGROUND:
    // iOS: App returning to foreground
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Received SDL_EVENT_WILL_ENTER_FOREGROUND");
    break;
  case SDL_EVENT_DID_ENTER_FOREGROUND:
    // iOS: App becomes interactive again
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Received SDL_EVENT_DID_ENTER_FOREGROUND");
    ctx->app_suspended = 0;
    if (ctx->device_connected) {
      m8_resume_processing();
    }
  case SDL_EVENT_WINDOW_RESIZED:
  case SDL_EVENT_WINDOW_MOVED:
    // If window size is changed, some operating systems might need a little nudge to fix scaling
    renderer_fix_texture_scaling_after_window_resize();
    break;

  // --- Input events ---
  case SDL_EVENT_GAMEPAD_ADDED:
  case SDL_EVENT_GAMEPAD_REMOVED:
    // Reinitialize game controllers on controller add/remove/remap
    gamecontrollers_initialize();
    break;

  case SDL_EVENT_KEY_DOWN:
    if (event->key.repeat > 0) {
      break;
    }

    // ALT+ENTER toggles fullscreen
    if (event->key.key == SDLK_RETURN && (event->key.mod & SDL_KMOD_ALT) > 0) {
      toggle_fullscreen();
      break;
    }

    // ALT+F4 quits program
    if (event->key.key == SDLK_F4 && (event->key.mod & SDL_KMOD_ALT) > 0) {
      key = (input_msg_s){special, msg_quit, 0, 0};
      break;
    }

    // ESC = toggle keyjazz
    if (event->key.key == SDLK_ESCAPE) {
      display_keyjazz_overlay(toggle_input_keyjazz(), keyjazz_base_octave, keyjazz_velocity);
      break;
    }

    key = handle_m8_buttons(event, &ctx->conf);
    SDL_Log("key %d",key.value);
    if (keyjazz_enabled) {
      key = handle_keyjazz(event, key.value, &ctx->conf);
    }
    if (key.type == normal) {
      keycode |= key.value;
    } else {
      keycode = key.value;
    }
    break;

  case SDL_EVENT_KEY_UP:

    key = handle_m8_buttons(event, &ctx->conf);
    if (keyjazz_enabled) {
      key = handle_keyjazz(event, key.value, &ctx->conf);
    }

    if (key.type == normal)
      keycode &= ~key.value;
    else
      keycode = 0;

    break;

  default:
    break;
  }
  return ret_val;
}

// Handles SDL input events
static void handle_sdl_events(config_params_s *conf) {

  static int prev_key_analog = 0;

  SDL_Event event;

  // Read joysticks
  const int key_analog = gamecontrollers_handle_buttons(conf);
  if (prev_key_analog != key_analog) {
    keycode = key_analog;
    prev_key_analog = key_analog;
  }

  const input_msg_s gamepad_msg = gamecontrollers_handle_special_messages(conf);
  if (gamepad_msg.type == special) {
    key = gamepad_msg;
  }
}

int input_process(config_params_s *conf, enum app_state *app_state) {
  static uint8_t prev_input = 0;
  static uint8_t prev_note = 0;

  // get current inputs
  const input_msg_s input = input_get_msg(conf);

  switch (input.type) {
  case normal:
    if (input.value != prev_input) {
      prev_input = input.value;
      m8_send_msg_controller(input.value);
    }
    break;
  case keyjazz:
    if (input.value != 0) {
      if (input.eventType == SDL_EVENT_KEY_DOWN && input.value != prev_input) {
        m8_send_msg_keyjazz(input.value, input.value2);
        prev_note = input.value;
      } else if (input.eventType == SDL_EVENT_KEY_UP && input.value == prev_note) {
        m8_send_msg_keyjazz(0xFF, 0);
      }
    }
    prev_input = input.value;
    break;
  case special:
    if (input.value != prev_input) {
      prev_input = input.value;
      switch (input.value) {
      case msg_quit:
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Received msg_quit from input device.");
        *app_state = 0;
        break;
      case msg_reset_display:
        m8_reset_display();
        break;
      case msg_toggle_audio:
        conf->audio_enabled = !conf->audio_enabled;
        audio_toggle(conf->audio_device_name, conf->audio_buffer_size);
        break;
      default:
        break;
      }
      break;
    }
  }
  return 1;
}

// Returns the currently pressed keys to main
input_msg_s input_get_msg(config_params_s *conf) {

  key = (input_msg_s){normal, 0, 0, 0};

  // Query for SDL events
  handle_sdl_events(conf);

  if (!keyjazz_enabled && keycode == (key_start | key_select | key_opt | key_edit)) {
    key = (input_msg_s){special, msg_reset_display, 0, 0};
  }

  if (key.type == normal) {
    /* Normal input keys go through some event-based manipulation in
       handle_sdl_events(), the value is stored in keycode variable */
    const input_msg_s input = (input_msg_s){key.type, keycode, 0, 0};
    return input;
  }
  // Special event keys already have the correct keycode baked in
  return key;
}
