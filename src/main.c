// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

/* Uncomment this line to enable debug messages or call make with `make
   CFLAGS=-DDEBUG_MSG` */
// #define DEBUG_MSG

#include <SDL3/SDL.h>
#include <signal.h>

#include "SDL2_inprint.h"
#include "audio.h"
#include "command.h"
#include "config.h"
#include "gamecontrollers.h"
#include "input.h"
#include "midi.h"
#include "render.h"
#include "serial.h"

enum state { QUIT, WAIT_FOR_DEVICE, RUN };

enum state run = WAIT_FOR_DEVICE;
uint8_t need_display_reset = 0;

// Handles CTRL+C / SIGINT
void signal_handler() { run = QUIT; }

int process_inputs(config_params_s conf) {
  static uint8_t prev_input = 0;
  static uint8_t prev_note = 0;

  // get current inputs
  const input_msg_s input = get_input_msg(&conf);

  switch (input.type) {
  case normal:
    if (input.value != prev_input) {
      prev_input = input.value;
      send_msg_controller(input.value);
    }
    break;
  case keyjazz:
    if (input.value != 0) {
      if (input.eventType == SDL_EVENT_KEY_DOWN && input.value != prev_input) {
        send_msg_keyjazz(input.value, input.value2);
        prev_note = input.value;
      } else if (input.eventType == SDL_EVENT_KEY_UP && input.value == prev_note) {
        send_msg_keyjazz(0xFF, 0);
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
        run = 0;
        break;
      case msg_reset_display:
        reset_display();
        break;
      case msg_toggle_audio:
        conf.audio_enabled = !conf.audio_enabled;
        toggle_audio(conf.audio_device_name, conf.audio_buffer_size);
        break;
      default:
        break;
      }
      break;
    }
  }
  return 1;
}

int main(const int argc, char *argv[]) {

  char *preferred_device = NULL;

#ifdef USE_LIBSERIALPORT // Device selection should be only available with libserialport
  if (argc == 2 && SDL_strcmp(argv[1], "--list") == 0) {
    return list_devices();
  }

  if (argc == 3 && SDL_strcmp(argv[1], "--dev") == 0) {
    preferred_device = argv[2];
    SDL_Log("Using preferred device %s.\n", preferred_device);
  }
#endif

  char *config_filename = NULL;
  if (argc == 3 && SDL_strcmp(argv[1], "--config") == 0) {
    config_filename = argv[2];
    SDL_Log("Using config file %s.\n", config_filename);
  }

  // Initialize the config to defaults read in the params from the
  // configfile if present
  config_params_s conf = init_config(config_filename);
  read_config(&conf);

  uint8_t port_inited = 0;

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
#ifdef SIGQUIT
  signal(SIGQUIT, signal_handler);
#endif

  // First device detection to avoid SDL init if it isn't necessary. To be run
  // only if we shouldn't wait for M8 to be connected.
  if (conf.wait_for_device == 0) {
    port_inited = init_serial(1, preferred_device);
    if (port_inited == 0) {
      return 1;
    }
  }

  // initialize all SDL systems
  if (initialize_sdl(conf.init_fullscreen) == false) {
    SDL_Quit();
    return 1;
  }
  run = QUIT;

  // initial scan for (existing) gamepads
  gamecontrollers_initialize();

#ifdef DEBUG_MSG
  SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
#endif

  // main loop begin
  do {
    // try to init serial port
    port_inited = init_serial(1, preferred_device);
    // if port init was successful, try to enable and reset display
    if (port_inited == 1 && enable_and_reset_display() == 1) {
      // if audio routing is enabled, try to initialize audio devices
      if (conf.audio_enabled == 1) {
        audio_init(conf.audio_device_name, conf.audio_buffer_size);
        // if audio is enabled, reset the display for second time to avoid glitches
        reset_display();
      }
      run = RUN;
    } else {
      SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Device not detected on begin loop.");
      if (conf.wait_for_device == 1) {
        run = WAIT_FOR_DEVICE;
      } else {
        run = QUIT;
      }
    }

    // wait until device is connected
    if (conf.wait_for_device == 1) {
      static uint32_t ticks_poll_device = 0;
      static uint32_t ticks_update_screen = 0;

      if (port_inited == 0) {
        screensaver_init();
      }

      while (run == WAIT_FOR_DEVICE) {
        // get current input
        const input_msg_s input = get_input_msg(&conf);
        if (input.type == special && input.value == msg_quit) {
          SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Input message QUIT.");
          run = QUIT;
        }

        if (SDL_GetTicks() - ticks_update_screen > 16) {
          ticks_update_screen = SDL_GetTicks();
          screensaver_draw();
          render_screen();
        }

        // Poll for M8 device every second
        if (port_inited == 0 && SDL_GetTicks() - ticks_poll_device > 1000) {
          ticks_poll_device = SDL_GetTicks();
          if (run == WAIT_FOR_DEVICE && init_serial(0, preferred_device) == 1) {

            if (conf.audio_enabled == 1) {
              if (audio_init(conf.audio_device_name, conf.audio_buffer_size) == 0) {
                SDL_Log("Cannot initialize audio");
                conf.audio_enabled = 0;
              }
            }

            const int result = enable_and_reset_display();
            // Device was found; enable display and proceed to the main loop
            if (result == 1) {
              run = RUN;
              port_inited = 1;
              screensaver_destroy();
            } else {
              SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Device not detected.");
              run = QUIT;
              screensaver_destroy();
            }
          }
        }
        SDL_Delay(conf.idle_ms);
      }
    } else {
      // classic startup behaviour, exit if device is not found
      if (port_inited == 0) {
        if (conf.audio_enabled == 1) {
          audio_destroy();
        }
        gamecontrollers_close();
        close_renderer();
        kill_inline_font();
        SDL_Quit();
        return -1;
      }
    }

    // main loop
    while (run == RUN) {
      process_inputs(conf);
      const int result = process_serial(conf);
      if (result == 0) {
        port_inited = 0;
        run = WAIT_FOR_DEVICE;
        audio_destroy();
      } else if (result == -1) {
        run = QUIT;
      }
      render_screen();
    }
  } while (run > QUIT);
  // main loop end

  // exit, clean up
  SDL_Log("Shutting down");
  audio_destroy();
  gamecontrollers_close();
  close_renderer();
  if (port_inited == 1)
    destroy_serial();
  SDL_Quit();
  return 0;
}
