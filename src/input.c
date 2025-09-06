//
// Created by Jonne Kokkonen on 15.4.2025.
//
#include "input.h"
#include "backends/audio.h"
#include "backends/m8.h"
#include "common.h"
#include "render.h"
#include <SDL3/SDL.h>

static unsigned char keyjazz_enabled = 0;
static unsigned char keyjazz_base_octave = 2;
static unsigned char keyjazz_velocity = 0x7F;

static unsigned char keycode = 0; // value of the pressed key
static input_msg_s key = {normal, 0, 0};

// Store gamepad state
static struct {
  int current_buttons;
  int analog_values[SDL_GAMEPAD_AXIS_COUNT];
} gamepad_state = {0};

static unsigned char toggle_input_keyjazz() {
  keyjazz_enabled = !keyjazz_enabled;
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, keyjazz_enabled ? "Keyjazz enabled" : "Keyjazz disabled");
  return keyjazz_enabled;
}

// Get note value for a scancode, or -1 if not found
static int get_note_for_scancode(const SDL_Scancode scancode) {

  // Map from SDL scancodes to note offsets
  const struct keyjazz_scancodes_t {
    SDL_Scancode scancode;
    unsigned char note_offset;
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

static input_msg_s handle_keyjazz(const SDL_Event *event, unsigned char keyvalue,
                                  const config_params_s *conf) {
  input_msg_s key = {keyjazz, keyvalue, keyjazz_velocity};

  // Check if this is a note key
  const int note_value = get_note_for_scancode(event->key.scancode);
  if (note_value >= 0) {
    SDL_Log("vel %d", keyjazz_velocity);
    key.value = note_value;
    return key;
  }

  // Not a note key, handle other settings
  key.type = normal;
  handle_keyjazz_settings(event, conf);

  return key;
}

/**
 * Handles key inputs based on SDL events and predefined configuration mappings.
 * Maps SDL keyboard events to corresponding input messages.
 * Provides a default message if no matching key mapping is found.
 *
 * @param event Pointer to the SDL_Event structure representing the current keyboard event.
 * @param conf Pointer to the config_params_s structure containing key mapping configurations.
 * @return An input_msg_s structure corresponding to the processed key input or a default message if
 * no match is found.
 */
static input_msg_s handle_m8_keys(const SDL_Event *event, const config_params_s *conf) {
  // Default message with a normal type and no value
  input_msg_s key = {normal, 0, 0};

  // Get the current scancode
  const SDL_Scancode scancode = event->key.scancode;

  // Handle standard keycodes (single key mapping)
  const struct {
    SDL_Scancode scancode;
    unsigned char value;
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

  // Check normal key mappings
  for (size_t i = 0; i < sizeof(normal_key_map) / sizeof(normal_key_map[0]); i++) {
    if (scancode == normal_key_map[i].scancode) {
      key.value = normal_key_map[i].value;
      return key;
    }
  }

  // No matching key found, return the default key message
  return key;
}

/**
 * Handles the key down events during the application runtime.
 *
 * @param ctx Pointer to the app_context structure
 * @param event Pointer to the SDL_Event structure containing data about the key down
 * event, including key and modifier states.
 */
void input_handle_key_down_event(struct app_context *ctx, const SDL_Event *event) {
  if (event->key.repeat > 0) {
    return;
  }

  if (event->key.key == SDLK_RETURN && (event->key.mod & SDL_KMOD_ALT) > 0) {
    toggle_fullscreen();
    return;
  }
  if (event->key.key == SDLK_F4 && (event->key.mod & SDL_KMOD_ALT) > 0) {
    ctx->app_state = QUIT;
    return;
  }
  if (event->key.key == SDLK_ESCAPE) {
    display_keyjazz_overlay(toggle_input_keyjazz(), keyjazz_base_octave, keyjazz_velocity);
    return;
  }

  // Toggle in-app log overlay using config-defined key
  if (event->key.scancode == ctx->conf.key_toggle_log) {
    renderer_toggle_log_overlay();
    return;
  }

  if (event->key.scancode == ctx->conf.key_toggle_audio && ctx->device_connected) {
    ctx->conf.audio_enabled = !ctx->conf.audio_enabled;
    audio_toggle(ctx->conf.audio_device_name, ctx->conf.audio_buffer_size);
    return;
  }

  if (event->key.scancode == ctx->conf.key_reset && ctx->device_connected && !keyjazz_enabled) {
    m8_reset_display();
    return;
  }

  key = handle_m8_keys(event, &ctx->conf);
  if (keyjazz_enabled) {
    key = handle_keyjazz(event, key.value, &ctx->conf);
  }
  keycode = (key.type == normal) ? (keycode | key.value) : key.value;

  input_process_and_send(ctx);
}

/**
 * Handles the "key up" SDL event and processes the associated input.
 * Interprets the event based on key mappings (default and keyjazz mode) and updates the input
 * state. Sends the processed input message for further handling.
 *
 * @param ctx Pointer to the app_context structure containing application state and configurations.
 * @param event Pointer to the SDL_Event structure representing the "key up" event.
 */
void input_handle_key_up_event(const struct app_context *ctx, const SDL_Event *event) {
  key = handle_m8_keys(event, &ctx->conf);
  if (keyjazz_enabled) {
    key = handle_keyjazz(event, key.value, &ctx->conf);
  }
  keycode = (key.type == normal) ? (keycode & ~key.value) : 0;

  input_process_and_send(ctx);
}

void input_handle_gamepad_button(struct app_context *ctx, const SDL_GamepadButton button,
                                 const bool pressed) {
  const config_params_s *conf = &ctx->conf;
  static int prev_key_value = 0;
  int key_value = 0;

  // Handle standard buttons
  const struct {
    int button;
    unsigned char value;
  } normal_key_map[] = {
      {conf->gamepad_up, key_up},         {conf->gamepad_left, key_left},
      {conf->gamepad_down, key_down},     {conf->gamepad_right, key_right},
      {conf->gamepad_select, key_select}, {conf->gamepad_start, key_start},
      {conf->gamepad_opt, key_opt},       {conf->gamepad_edit, key_edit},
  };

  // Check normal key mappings
  for (size_t i = 0; i < sizeof(normal_key_map) / sizeof(normal_key_map[0]); i++) {
    if (button == normal_key_map[i].button) {
      key_value = normal_key_map[i].value;
    }
  }

  if (pressed && key_value != prev_key_value) {
    gamepad_state.current_buttons |= key_value;
  } else {
    gamepad_state.current_buttons &= ~key_value;
  }

  // Handle special button combinations
  if (gamepad_state.current_buttons == (key_start | key_select | key_opt | key_edit)) {
    m8_reset_display();
    return;
  }

  if (pressed && button == conf->gamepad_quit && gamepad_state.current_buttons == key_select) {
    ctx->app_state = QUIT;
    return;
  }

  keycode = gamepad_state.current_buttons;

  input_process_and_send(ctx);
}

/**
 * Helper function to update button states based on analog axis value.
 *
 * @param axis_value     The current value of the axis
 * @param threshold      The threshold that determines when a direction is activated
 * @param negative_key   The key to activate when the axis value is below a negative threshold
 * @param positive_key   The key to activate when the axis value is above a positive threshold
 */
static void update_button_state_from_axis(Sint16 axis_value, int threshold, int negative_key,
                                          int positive_key) {
  if (axis_value < -threshold) {
    gamepad_state.current_buttons |= negative_key;
  } else if (axis_value > threshold) {
    gamepad_state.current_buttons |= positive_key;
  } else {
    gamepad_state.current_buttons &= ~(negative_key | positive_key);
  }
}

/**
 * Processes gamepad axis movements and updates internal state accordingly.
 * Maps gamepad analog axis input to directional or functional button states
 * based on the configuration and analog value threshold.
 *
 * @param ctx Pointer to the app_context structure containing application configuration and state.
 * @param axis The gamepad axis being processed, specified as an SDL_GamepadAxis.
 * @param value The analog value of the axis, typically ranging from -32768 to 32767.
 */
void input_handle_gamepad_axis(const struct app_context *ctx, const SDL_GamepadAxis axis,
                               const Sint16 value) {
  const config_params_s *conf = &ctx->conf;
  gamepad_state.analog_values[axis] = value;

  // Process directional axes and update button states
  if (axis == conf->gamepad_analog_axis_updown) {
    update_button_state_from_axis(value, conf->gamepad_analog_threshold, key_up, key_down);
  } else if (axis == conf->gamepad_analog_axis_leftright) {
    update_button_state_from_axis(value, conf->gamepad_analog_threshold, key_left, key_right);
  } else if (axis == conf->gamepad_analog_axis_select) {
    update_button_state_from_axis(value, conf->gamepad_analog_threshold, key_select, key_select);
  } else if (axis == conf->gamepad_analog_axis_opt) {
    update_button_state_from_axis(value, conf->gamepad_analog_threshold, key_opt, key_opt);
  } else if (axis == conf->gamepad_analog_axis_start) {
    update_button_state_from_axis(value, conf->gamepad_analog_threshold, key_start, key_start);
  } else if (axis == conf->gamepad_analog_axis_edit) {
    update_button_state_from_axis(value, conf->gamepad_analog_threshold, key_edit, key_edit);
  }

  keycode = gamepad_state.current_buttons;

  input_process_and_send(ctx);
}

/**
 * Processes the current input message and sends the appropriate data based on its type.
 *
 * @param ctx Pointer to the app_context structure containing application configuration and state.
 * @return An integer indicating the success status of the operation.
 * Returns 1 for successful processing and sending of messages, or 0 if the device is not connected.
 */
int input_process_and_send(const struct app_context *ctx) {
  if (!ctx->device_connected) {
    return 0;
  }
  static unsigned char prev_input = 0;

  // get current inputs
  const input_msg_s input = (input_msg_s){key.type, keycode, 0};

  switch (input.type) {
  case normal:
    if (input.value != prev_input) {
      prev_input = input.value;
      m8_send_msg_controller(input.value);
    }
    break;
  case keyjazz:
    if (input.value != 0) {
      if (input.value != prev_input) {
        prev_input = input.value;
        m8_send_msg_keyjazz(input.value, keyjazz_velocity);
      }
    } else {
      m8_send_msg_keyjazz(0xFF, 0);
    }
    prev_input = input.value;
    break;
  default:;
  }
  return 1;
}