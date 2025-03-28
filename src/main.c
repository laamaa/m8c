// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

/* Uncomment this line to enable debug messages or call make with `make
   CFLAGS=-DDEBUG_MSG` */
// #define DEBUG_MSG

#include <SDL3/SDL.h>
#include <signal.h>

#include "SDL2_inprint.h"
#include "backends/audio.h"
#include "backends/m8.h"
#include "command.h"
#include "config.h"
#include "gamecontrollers.h"
#include "input.h"
#include "render.h"

#include <stdlib.h>

enum app_state app_state = WAIT_FOR_DEVICE;

// Handle CTRL+C / SIGINT, SIGKILL etc.
static void signal_handler(int unused) {
  (void)unused;
  app_state = QUIT;
}

static void initialize_signals() {
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
#ifdef SIGQUIT
  signal(SIGQUIT, signal_handler);
#endif
}


static void do_wait_for_device(const char *preferred_device, unsigned char *m8_connected,
                        config_params_s *conf) {
  static Uint64 ticks_poll_device = 0;
  static Uint64 ticks_update_screen = 0;

  if (*m8_connected == 0) {
    screensaver_init();
  }

  while (app_state == WAIT_FOR_DEVICE) {
    // get current input
    const input_msg_s input = input_get_msg(&*conf);
    if (input.type == special && input.value == msg_quit) {
      SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Input message QUIT.");
      app_state = QUIT;
    }

    if (SDL_GetTicks() - ticks_update_screen > 16) {
      ticks_update_screen = SDL_GetTicks();
      screensaver_draw();
      render_screen();
    }

    // Poll for M8 device every second
    if (*m8_connected == 0 && SDL_GetTicks() - ticks_poll_device > 1000) {
      ticks_poll_device = SDL_GetTicks();
      if (app_state == WAIT_FOR_DEVICE && m8_initialize(0, preferred_device) == 1) {

        if (conf->audio_enabled == 1) {
          if (audio_initialize(conf->audio_device_name, conf->audio_buffer_size) == 0) {
            SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Cannot initialize audio");
            conf->audio_enabled = 0;
          }
        }

        const int m8_enabled = m8_enable_and_reset_display();
        // Device was found; enable display and proceed to the main loop
        if (m8_enabled == 1) {
          app_state = RUN;
          *m8_connected = 1;
          screensaver_destroy();
        } else {
          SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Device not detected.");
          app_state = QUIT;
          screensaver_destroy();
        }
      }
    }
    SDL_Delay(conf->idle_ms);
  }
}

static config_params_s initialize_config(int argc, char *argv[], char **preferred_device, char **config_filename) {
  for (int i = 1; i < argc; i++) {
    if (SDL_strcmp(argv[i], "--list") == 0) {
      exit(m8_list_devices());
    }
    if (SDL_strcmp(argv[i], "--dev") == 0 && i + 1 < argc) {
      *preferred_device = argv[i + 1];
      SDL_Log("Using preferred device: %s.\n", *preferred_device);
      i++;
    } else if (SDL_strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
      *config_filename = argv[i + 1];
      SDL_Log("Using config file: %s.\n", *config_filename);
      i++;
    }
  }

  config_params_s conf = config_initialize(*config_filename);
  config_read(&conf);
  return conf;
}

static void cleanup_resources(const unsigned char device_connected, const config_params_s *conf) {
  if (conf->audio_enabled) {
    audio_close();
  }
  gamecontrollers_close();
  renderer_close();
  inline_font_close();
  if (device_connected) {
    m8_close();
  }
  SDL_Quit();
  SDL_Log("Shutting down.");
}

static unsigned char handle_device_initialization(unsigned char wait_for_device, const char *preferred_device) {
  const unsigned char device_connected = m8_initialize(1, preferred_device);
  if (!wait_for_device && device_connected == 0) {
    SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Device not detected!");
    exit(EXIT_FAILURE);
  }
  return device_connected;
}

static void main_loop(config_params_s *conf, const char *preferred_device) {
  unsigned char device_connected = 0;

  do {
    device_connected = m8_initialize(1, preferred_device);
    if (device_connected && m8_enable_and_reset_display()) {
      if (conf->audio_enabled) {
        audio_initialize(conf->audio_device_name, conf->audio_buffer_size);
        m8_reset_display();  // Avoid display glitches.
      }
      app_state = RUN;
    } else {
      SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Device not detected.");
      app_state = conf->wait_for_device ? WAIT_FOR_DEVICE : QUIT;
    }

    if (conf->wait_for_device && app_state == WAIT_FOR_DEVICE) {
      do_wait_for_device(preferred_device, &device_connected, conf);
    } else if (!device_connected && app_state != WAIT_FOR_DEVICE) {
      cleanup_resources(0, conf);
      exit(EXIT_FAILURE);
    }

    // Handle input, process data, and render screen while running.
    while (app_state == RUN) {
      input_process(conf, &app_state);
      const int result = m8_process_data(conf);
      if (result == DEVICE_DISCONNECTED) {
        device_connected = 0;
        app_state = WAIT_FOR_DEVICE;
        audio_close();
      } else if (result == DEVICE_FATAL_ERROR) {
        app_state = QUIT;
      }
      render_screen();
      SDL_Delay(conf->idle_ms);
    }
  } while (app_state > QUIT);

  cleanup_resources(device_connected, conf);
}


int main(const int argc, char *argv[]) {
  char *preferred_device = NULL;
  char *config_filename = NULL;

  config_params_s conf = initialize_config(argc, argv, &preferred_device, &config_filename);
  initialize_signals();

  const unsigned char initial_device_connected = handle_device_initialization(conf.wait_for_device, preferred_device);
  if (!renderer_initialize(conf.init_fullscreen)) {
    SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Failed to initialize renderer.");
    cleanup_resources(initial_device_connected, &conf);
    return EXIT_FAILURE;
  }

  gamecontrollers_initialize();

#ifndef NDEBUG
  SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);
  SDL_LogDebug(SDL_LOG_CATEGORY_TEST, "Running a Debug build");
#endif

  main_loop(&conf, preferred_device);
  return EXIT_SUCCESS;
}
