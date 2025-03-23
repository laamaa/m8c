// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

/* Uncomment this line to enable debug messages or call make with `make
   CFLAGS=-DDEBUG_MSG` */
// #define DEBUG_MSG

#include <SDL3/SDL.h>
#include <signal.h>

#include "SDL2_inprint.h"
#include "backends/audio.h"
#include "backends/rtmidi.h"
#include "backends/serialport.h"
#include "backends/usb.h"
#include "command.h"
#include "config.h"
#include "gamecontrollers.h"
#include "input.h"
#include "render.h"

enum app_state app_state = WAIT_FOR_DEVICE;

// Handle CTRL+C / SIGINT, SIGKILL etc.
void signal_handler(int unused) {
  (void)unused;
  app_state = QUIT;
}

void do_wait_for_device(const char *preferred_device, unsigned char *m8_connected,
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

int main(const int argc, char *argv[]) {

  char *preferred_device = NULL;
  unsigned char m8_connected = 0;

  if (argc == 2 && SDL_strcmp(argv[1], "--list") == 0) {
    return m8_list_devices();
  }

  if (argc == 3 && SDL_strcmp(argv[1], "--dev") == 0) {
    preferred_device = argv[2];
    SDL_Log("Using preferred device %s.\n", preferred_device);
  }

  char *config_filename = NULL;
  if (argc == 3 && SDL_strcmp(argv[1], "--config") == 0) {
    config_filename = argv[2];
    SDL_Log("Using config file %s.\n", config_filename);
  }

  // Initialize the config to defaults
  config_params_s conf = config_initialize(config_filename);
  // Read in the params from the configfile if present
  config_read(&conf);

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
#ifdef SIGQUIT
  signal(SIGQUIT, signal_handler);
#endif

  // First device detection to avoid SDL init if it isn't necessary. To be run
  // only if we shouldn't wait for M8 to be connected.
  if (conf.wait_for_device == 0) {
    m8_connected = m8_initialize(1, preferred_device);
    if (m8_connected == 0) {
      return 1;
    }
  }

  // initialize all SDL systems
  if (renderer_initialize(conf.init_fullscreen) == false) {
    SDL_Quit();
    return 1;
  }
  app_state = QUIT;

  // initial scan for (existing) gamepads
  gamecontrollers_initialize();

#ifndef NDEBUG
  SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);
  SDL_LogDebug(SDL_LOG_CATEGORY_TEST, "Running a Debug build");
#endif

  // main loop begin
  do {
    // try to init connection to M8
    m8_connected = m8_initialize(1, preferred_device);
    if (m8_connected == 1 && m8_enable_and_reset_display() == 1) {
      if (conf.audio_enabled == 1) {
        audio_initialize(conf.audio_device_name, conf.audio_buffer_size);
        // if audio is enabled, reset the display for second time to avoid glitches
        m8_reset_display();
      }
      app_state = RUN;
    } else {
      SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Device not detected on initial check.");
      if (conf.wait_for_device == 1) {
        app_state = WAIT_FOR_DEVICE;
      } else {
        app_state = QUIT;
      }
    }

    // wait until device is connected
    if (conf.wait_for_device == 1) {
      do_wait_for_device(preferred_device, &m8_connected, &conf);
    } else {
      // classic startup behaviour, exit if device is not found
      if (m8_connected == 0) {
        if (conf.audio_enabled == 1) {
          audio_close();
        }
        gamecontrollers_close();
        renderer_close();
        inline_font_close();
        SDL_Quit();
        return -1;
      }
    }

    // main loop
    while (app_state == RUN) {
      input_process(conf, &app_state);
      const int result = m8_process_data(conf);
      if (result == 0) {
        // Device disconnected
        m8_connected = 0;
        app_state = WAIT_FOR_DEVICE;
        audio_close();
      } else if (result == -1) {
        // Fatal error
        app_state = QUIT;
      }
      render_screen();
      SDL_Delay(conf.idle_ms);
    }
  } while (app_state > QUIT);
  // Main loop end

  // Exit, clean up
  SDL_Log("Shutting down");
  audio_close();
  gamecontrollers_close();
  renderer_close();
  if (m8_connected == 1)
    m8_close();
  SDL_Quit();
  return 0;
}
