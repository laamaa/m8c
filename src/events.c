#include "events.h"
#include "backends/m8.h"
#include "common.h"
#include "gamepads.h"
#include "input.h"
#include "render.h"
#include "settings.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <limits.h>

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  struct app_context *ctx = appstate;
  SDL_AppResult ret_val = SDL_APP_CONTINUE;

  switch (event->type) {

  // --- System events ---
  case SDL_EVENT_QUIT:
  case SDL_EVENT_TERMINATING:
    ret_val = SDL_APP_SUCCESS;
    break;
  case SDL_EVENT_WINDOW_RESIZED:
  case SDL_EVENT_WINDOW_MOVED:
    // If the window size is changed, some systems might need a little nudge to fix scaling
    renderer_fix_texture_scaling_after_window_resize(&ctx->conf);

    if (ctx->conf.persist_window_position_and_size == 1) {
      window_position_and_size_s window_position_and_size = {
          .height = INT_MIN, .width = INT_MIN, .x = INT_MIN, .y = INT_MIN};

      if (event->window.type == SDL_EVENT_WINDOW_MOVED) {
        window_position_and_size.x = event->window.data1;
        window_position_and_size.y = event->window.data2;
      }

      if (event->window.type == SDL_EVENT_WINDOW_RESIZED) {
        window_position_and_size.height = event->window.data2;
        window_position_and_size.width = event->window.data1;
      }

      update_and_write_window_position_and_size_config(&ctx->conf, &window_position_and_size);
    }

    break;

  // --- iOS specific events ---
  case SDL_EVENT_DID_ENTER_BACKGROUND:
    // iOS: Application entered into the background on iOS. About 5 seconds to stop things.
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Received SDL_EVENT_DID_ENTER_BACKGROUND");
    ctx->app_suspended = 1;
    if (ctx->device_connected)
      m8_pause_processing();
    break;
  case SDL_EVENT_WILL_ENTER_BACKGROUND:
    // iOS: App about to enter into the background
    break;
  case SDL_EVENT_WILL_ENTER_FOREGROUND:
    // iOS: App returning to the foreground
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Received SDL_EVENT_WILL_ENTER_FOREGROUND");
    break;
  case SDL_EVENT_DID_ENTER_FOREGROUND:
    // iOS: App becomes interactive again
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Received SDL_EVENT_DID_ENTER_FOREGROUND");
    ctx->app_suspended = 0;
    if (ctx->device_connected) {
      renderer_clear_screen();
      m8_resume_processing();
    }
    break;

  // --- Input events ---
  case SDL_EVENT_GAMEPAD_ADDED:
  case SDL_EVENT_GAMEPAD_REMOVED:
    // Reinitialize game controllers on controller add/remove/remap
    gamepads_initialize();
    break;

  case SDL_EVENT_KEY_DOWN:
    // Settings view toggles handled here to avoid being able to get stuck in the config view
    // Toggle settings with Command/Win+comma (for keyboards without function keys)
    if (event->key.key == SDLK_COMMA && event->key.repeat == 0 && (event->key.mod & SDL_KMOD_GUI)) {
      settings_toggle_open();
      return ret_val;
    }
    // Toggle settings with config defined key
    if (event->key.scancode == ctx->conf.key_toggle_settings && event->key.repeat == 0) {
      settings_toggle_open();
      return ret_val;
    }
    // Route to settings if open
    if (settings_is_open()) {
      settings_handle_event(ctx, event);
      return ret_val;
    }
    input_handle_key_down_event(ctx, event);
    break;

  case SDL_EVENT_KEY_UP:
    if (settings_is_open()) {
      settings_handle_event(ctx, event);
      return ret_val;
    }
    input_handle_key_up_event(ctx, event);
    break;

  case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    if (settings_is_open()) {
      settings_handle_event(ctx, event);
      return ret_val;
    }

    // Allow toggling the settings view using a gamepad only when the device is disconnected to
    // avoid accidentally opening the screen while using the device
    if (event->gbutton.button == SDL_GAMEPAD_BUTTON_BACK) {
      if (ctx->app_state == WAIT_FOR_DEVICE && !settings_is_open()) {
        settings_toggle_open();
      }
    }

    input_handle_gamepad_button(ctx, event->gbutton.button, true);
    break;

  case SDL_EVENT_GAMEPAD_BUTTON_UP:
    if (settings_is_open()) {
      settings_handle_event(ctx, event);
      return ret_val;
    }
    input_handle_gamepad_button(ctx, event->gbutton.button, false);
    break;

  case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    if (settings_is_open()) {
      settings_handle_event(ctx, event);
      return ret_val;
    }
    input_handle_gamepad_axis(ctx, event->gaxis.axis, event->gaxis.value);
    break;

  default:
    break;
  }
  return ret_val;
}
