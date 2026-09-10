// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

/* Uncomment this line to enable debug messages or call make with `make
   CFLAGS=-DDEBUG_MSG` */
// #define DEBUG_MSG

#define APP_VERSION "v2.2.4"

#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <stdlib.h>

#include "SDL2_inprint.h"
#include "backends/audio.h"
#include "backends/m8.h"
#include "common.h"
#include "config.h"
#include "gamepads.h"
#include "render.h"
#include "log_overlay.h"

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
  render_screen(&ctx->conf);

  // Discovery is throttled; an open MIDI handshake is polled once per frame.
  if (!ctx->device_initializing) {
    if (SDL_GetTicks() < ticks_poll_device) {
      return;
    }
    ticks_poll_device = SDL_GetTicks() + 1000;
    ctx->device_initializing = m8_initialize(0, ctx->preferred_device);
    if (!ctx->device_initializing) {
      return;
    }
  }

  const int result = m8_enable_display(1);
  if (result == DEVICE_CONNECTING) {
    return;
  }
  ctx->device_initializing = 0;
  if (result != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "M8 remote display did not respond; will retry.");
    m8_close();
    ticks_poll_device = SDL_GetTicks() + 3000;
    return;
  }

  ctx->device_connected = 1;
  ctx->app_state = RUN;
  screensaver_destroy();
  screensaver_initialized = 0;
  if (ctx->conf.audio_enabled) {
    if (!audio_initialize(ctx->conf.audio_device_name, ctx->conf.audio_buffer_size)) {
      SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Cannot initialize audio");
      ctx->conf.audio_enabled = 0;
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

  if (TARGET_OS_IOS == 1) {
    // Predefined settings for iOS
    conf.init_fullscreen = 1;
  }
  config_read(&conf);

  return conf;
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
    do_wait_for_device(ctx);
    break;

  case RUN: {
    // Handle app suspension
    if (ctx->app_suspended) {
      return SDL_APP_CONTINUE;
    }
    const int result = m8_process_data(&ctx->conf);
    if (result == DEVICE_DISCONNECTED) {
      ctx->device_connected = 0;
      ctx->app_state = WAIT_FOR_DEVICE;
      audio_close();
    } else if (result == DEVICE_FATAL_ERROR) {
      return SDL_APP_FAILURE;
    }
    render_screen(&ctx->conf);
    break;
  }

  case QUIT:
    app_result = SDL_APP_SUCCESS;
    break;
  }

  return app_result;
}

// Initialize the app: initialize context, configs, renderer controllers and attempt to find M8
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
  SDL_SetAppMetadata("M8C",APP_VERSION,"fi.laamaa.m8c");

  char *config_filename = NULL;

  // Initialize in-app log capture/overlay
  log_overlay_init();

#ifndef NDEBUG
  // Show debug messages in the application log
  SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);
  SDL_LogDebug(SDL_LOG_CATEGORY_TEST, "Running a Debug build");
#else
  // Show debug messages in the application log
  SDL_SetLogPriorities(SDL_LOG_PRIORITY_INFO);
#endif

  // Process the application's main callback roughly at 120 Hz
  SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "120");

  struct app_context *ctx = SDL_calloc(1, sizeof(struct app_context));
  if (ctx == NULL) {
    SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "SDL_calloc failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  *appstate = ctx;
  ctx->app_state = INITIALIZE;
  ctx->conf = initialize_config(argc, argv, &ctx->preferred_device, &config_filename);

  if (!renderer_initialize(&ctx->conf)) {
    SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Failed to initialize renderer.");
    return SDL_APP_FAILURE;
  }

  ctx->device_initializing = m8_initialize(1, ctx->preferred_device);

  if (gamepads_initialize() < 0) {
    SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Failed to initialize game controllers.");
    return SDL_APP_FAILURE;
  }

  ctx->app_state = WAIT_FOR_DEVICE;

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  (void)result; // Suppress compiler warning

  struct app_context *app = appstate;

  if (app) {
    if (app->app_state == WAIT_FOR_DEVICE) {
      screensaver_destroy();
    }
    if (app->conf.audio_enabled) {
      audio_close();
    }
    gamepads_close();
    renderer_close();
    inline_font_close();
#ifdef USE_RTMIDI
    m8_close(); // Discovery can own MIDI clients even when no port was found.
#else
    if (app->device_connected || app->device_initializing) {
      m8_close();
    }
#endif
    SDL_free(app);

    SDL_Log("Shutting down.");
    SDL_Quit();
  }
}