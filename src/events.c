#include "events.h"
#include "backends/m8.h"
#include "common.h"
#include "gamepads.h"
#include "input.h"
#include "render.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  struct app_context *ctx = appstate;
  SDL_AppResult ret_val = SDL_APP_CONTINUE;

  switch (event->type) {

  // --- System events ---
  case SDL_EVENT_QUIT:
  case SDL_EVENT_TERMINATING:
    ret_val = SDL_APP_SUCCESS;
    break;
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
  case SDL_EVENT_WINDOW_RESIZED:
  case SDL_EVENT_WINDOW_MOVED:
    // If the window size is changed, some operating systems might need a little nudge to fix scaling
    renderer_fix_texture_scaling_after_window_resize();
    break;

  // --- Input events ---
  case SDL_EVENT_GAMEPAD_ADDED:
  case SDL_EVENT_GAMEPAD_REMOVED:
    // Reinitialize game controllers on controller add/remove/remap
    gamepads_initialize();
    break;

  case SDL_EVENT_KEY_DOWN:
    input_handle_key_down_event(ctx, event);
    break;

  case SDL_EVENT_KEY_UP:
    input_handle_key_up_event(ctx, event);
    break;

  case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    input_handle_gamepad_button(ctx, event->gbutton.button, true);
    break;

  case SDL_EVENT_GAMEPAD_BUTTON_UP:
    input_handle_gamepad_button(ctx, event->gbutton.button, false);
    break;

  case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    input_handle_gamepad_axis(ctx, event->gaxis.axis, event->gaxis.value);
    break;


  default:
    break;
  }
  return ret_val;
}
