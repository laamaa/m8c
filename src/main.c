// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

/* Uncomment this line to enable debug messages or call make with `make
   CFLAGS=-DDEBUG_MSG` */
// #define DEBUG_MSG

#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <stdlib.h>

#include "SDL2_inprint.h"
#include "backends/audio.h"
#include "backends/m8.h"
#include "common.h"
#include "config.h"
#include "events.h"
#include "gamecontrollers.h"
#include "render.h"

static void do_wait_for_device(struct app_context *ctx) {
  static Uint64 ticks_poll_device = 0;
  static int screensaver_initialized = 0;

  // Handle app suspension
  if (ctx->app_suspended) {
    return;
  }

  if (!screensaver_initialized) {
    screensaver_initialized = screensaver_init();
  }
  screensaver_draw();
  render_screen();

  // Poll for M8 device every second
  if (ctx->device_connected == 0 && SDL_GetTicks() - ticks_poll_device > 1000) {
    ticks_poll_device = SDL_GetTicks();
    if (m8_initialize(0, ctx->preferred_device)) {

      if (ctx->conf.audio_enabled) {
        if (!audio_initialize(ctx->conf.audio_device_name, ctx->conf.audio_buffer_size)) {
          SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Cannot initialize audio");
          ctx->conf.audio_enabled = 0;
        }
      }

      const int m8_enabled = m8_enable_display(0);
      // Device was found; enable display and proceed to the main loop
      if (m8_enabled == 1) {
        ctx->app_state = RUN;
        ctx->device_connected = 1;
        screensaver_destroy();
        screensaver_initialized = 0;
        // renderer_clear_screen();
        SDL_Log("Device connected.");
        SDL_Delay(100);     // Give the device time to initialize
        m8_reset_display(); // Avoid display glitches.
      } else {
        SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Device not detected.");
        ctx->app_state = QUIT;
        screensaver_destroy();
        screensaver_initialized = 0;
#ifdef USE_RTMIDI
        show_error_message(
            "Cannot initialize M8 remote display. Make sure you're running "
            "firmware 6.0.0 or newer. Please close and restart the application to try again.");
#endif
      }
    }
  }
}

static config_params_s initialize_config(int argc, char *argv[], char **preferred_device,
                                         char **config_filename) {
  for (int i = 1; i < argc; i++) {
    if (SDL_strcmp(argv[i], "--list") == 0) {
      exit(m8_list_devices());
    }
    if (SDL_strcmp(argv[i], "--dev") == 0 && i + 1 < argc) {
      *preferred_device = argv[i + 1];
      SDL_Log("Using preferred device: %s", *preferred_device);
      i++;
    } else if (SDL_strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
      *config_filename = argv[i + 1];
      SDL_Log("Using config file: %s", *config_filename);
      i++;
    }
  }

  config_params_s conf = config_initialize(*config_filename);
#ifndef TARGET_OS_IOS
  // It's not possible to edit the config on iOS so let's just go with the defaults
  config_read(&conf);
#endif
  return conf;
}

/**
 * Handles the initialization of a device and verifies its connection state.
 *
 * @param wait_for_device A flag indicating whether the system should wait for the device to
 * connect. If set to 0 and the device is not detected, the application exits.
 * @param preferred_device A string representing the preferred device to initialize.
 * @return An unsigned char indicating the connection state of the device.
 *         Returns 1 if the device is connected successfully, or 0 if not connected.
 */
static unsigned char handle_m8_connection_init(const unsigned char wait_for_device,
                                                  const char *preferred_device) {
  const unsigned char device_connected = m8_initialize(1, preferred_device);
  if (!wait_for_device && device_connected == 0) {
    SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Device not detected!");
    exit(EXIT_FAILURE);
  }
  return device_connected;
}

// Main callback loop - read inputs, process data from the device, render screen
SDL_AppResult SDL_AppIterate(void *appstate) {
  if (appstate == NULL) {
    return SDL_APP_FAILURE;
  }

  struct app_context *ctx = appstate;
  SDL_AppResult app_result = SDL_APP_CONTINUE;

  switch (ctx->app_state) {
  case INITIALIZE:
    break;

  case WAIT_FOR_DEVICE:
    if (ctx->conf.wait_for_device) {
      do_wait_for_device(ctx);
    }
    break;

  case RUN:
    const int result = m8_process_data(&ctx->conf);
    if (result == DEVICE_DISCONNECTED) {
      ctx->device_connected = 0;
      ctx->app_state = WAIT_FOR_DEVICE;
      audio_close();
    } else if (result == DEVICE_FATAL_ERROR) {
      return SDL_APP_FAILURE;
    }
    render_screen();
    break;

  case QUIT:
    app_result = SDL_APP_SUCCESS;
    break;
  }

  return app_result;
}

// Initialize the app: initialize context, configs, renderer controllers and attempt to find M8
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
  char *config_filename = NULL;

  struct app_context *ctx = SDL_calloc(1, sizeof(struct app_context));
  if (ctx == NULL) {
    SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "SDL_calloc failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  *appstate = ctx;
  ctx->app_state = INITIALIZE;

  ctx->conf = initialize_config(argc, argv, &ctx->preferred_device, &config_filename);
  ctx->device_connected =
      handle_m8_connection_init(ctx->conf.wait_for_device, ctx->preferred_device);

  if (!renderer_initialize(&ctx->conf)) {
    SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Failed to initialize renderer.");
    return SDL_APP_FAILURE;
  }

#ifndef NDEBUG
  // Show debug messages in the application log
  SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);
  SDL_LogDebug(SDL_LOG_CATEGORY_TEST, "Running a Debug build");
#endif

  if (gamecontrollers_initialize() < 0) {
    SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Failed to initialize game controllers.");
    return SDL_APP_FAILURE;
  }

  // Process the application's main callback roughly at 120 Hz
  SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "120");

  if (ctx->device_connected && m8_enable_display(0)) {
    if (ctx->conf.audio_enabled) {
      audio_initialize(ctx->conf.audio_device_name, ctx->conf.audio_buffer_size);
    }
    ctx->app_state = RUN;
    m8_reset_display(); // Avoid display glitches.
  } else {
    SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Device not detected.");
    ctx->device_connected = 0;
    ctx->app_state = ctx->conf.wait_for_device ? WAIT_FOR_DEVICE : QUIT;
  }

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  struct app_context *app = appstate;

  if (app) {
    if (app->app_state == WAIT_FOR_DEVICE) {
      screensaver_destroy();
    }
    if (app->conf.audio_enabled) {
      audio_close();
    }
    gamecontrollers_close();
    renderer_close();
    inline_font_close();
    if (app->device_connected) {
      m8_close();
    }
    SDL_free(app);

    SDL_Log("Shutting down.");
    SDL_Quit();
  }
}