// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

/* Uncomment this line to enable debug messages or call make with `make
   CFLAGS=-DDEBUG_MSG` */
// #define DEBUG_MSG

#include <SDL3/SDL.h>
#include <signal.h>
#include <stdlib.h>

#include "SDL2_inprint.h"
#include "backends/audio.h"
#include "backends/m8.h"
#include "command.h"
#include "config.h"
#include "gamecontrollers.h"
#include "input.h"
#include "render.h"

#if TARGET_OS_IOS
#include <SDL3/SDL_main.h>
unsigned char app_suspended = 0;
#endif // TARGET_OS_IOS

enum app_state app_state = WAIT_FOR_DEVICE;
unsigned char device_connected = 0;

// Handle CTRL+C / SIGINT, SIGKILL etc.
static void signal_handler(int unused) {
  (void)unused;
  app_state = QUIT;
}

static void initialize_signals(void) {
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
#ifdef SIGQUIT // Not available on Windows.
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

#if TARGET_OS_IOS
    // Handle app suspension
    if (app_suspended) {
      SDL_Delay(conf->idle_ms);
      continue;
    }
#endif // TARGET_OS_IOS

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
#ifdef USE_RTMIDI
          show_error_message("Cannot initialize M8 remote display. Make sure you're running "
                             "firmware 6.0.0 or newer. Please close and restart the application to try again.");
#endif
        }
      }
    }
    SDL_Delay(conf->idle_ms);
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

static unsigned char handle_device_initialization(const unsigned char wait_for_device,
                                                  const char *preferred_device) {
  const unsigned char device_connected = m8_initialize(1, preferred_device);
  if (!wait_for_device && device_connected == 0) {
    SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Device not detected!");
    exit(EXIT_FAILURE);
  }
  return device_connected;
}

#if TARGET_OS_IOS
// IOS events handler
static bool SDLCALL handle_app_events(void *userdata, SDL_Event *event) {
  const config_params_s *conf = (config_params_s *)userdata;
  switch (event->type) {
  case SDL_EVENT_TERMINATING:
    /* Terminate the app.
       Shut everything down before returning from this function.
    */
    cleanup_resources(device_connected, conf);
    return 0;
  case SDL_EVENT_DID_ENTER_BACKGROUND:
    /* This will get called if the user accepted whatever sent your app to the background.
       If the user got a phone call and canceled it, you'll instead get an
       SDL_EVENT_DID_ENTER_FOREGROUND event and restart your loops. When you get this, you have 5
       seconds to save all your state or the app will be terminated. Your app is NOT active at this
       point.
    */
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Received SDL_EVENT_DID_ENTER_BACKGROUND");
    app_suspended = 1;
    if (device_connected)
      m8_pause_processing();
    return 0;
  case SDL_EVENT_LOW_MEMORY:
    /* You will get this when your app is paused and iOS wants more memory.
       Release as much memory as possible.
    */
    return 0;
  case SDL_EVENT_WILL_ENTER_BACKGROUND:
    /* Prepare your app to go into the background.  Stop loops, etc.
       This gets called when the user hits the home button, or gets a call.
    */
    return 0;
  case SDL_EVENT_WILL_ENTER_FOREGROUND:
    /* This call happens when your app is coming back to the foreground.
       Restore all your state here.
    */
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Received SDL_EVENT_WILL_ENTER_FOREGROUND");
    app_suspended = 0;
    if (device_connected)
      m8_resume_processing();
    return 0;
  case SDL_EVENT_DID_ENTER_FOREGROUND:
    /* Restart your loops here.
       Your app is interactive and getting CPU again.
    */
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Received SDL_EVENT_DID_ENTER_FOREGROUND");
    return 0;
  default:
    /* No special processing, add it to the event queue */
    return 1;
  }
}
#endif // TARGET_OS_IOS

static void main_loop(config_params_s *conf, const char *preferred_device) {

  do {
    if (!device_connected) {
      device_connected = m8_initialize(1, preferred_device);
    }
    if (device_connected && m8_enable_and_reset_display()) {
      if (conf->audio_enabled) {
        audio_initialize(conf->audio_device_name, conf->audio_buffer_size);
        m8_reset_display(); // Avoid display glitches.
      }
      app_state = RUN;
    } else {
      SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Device not detected.");
      device_connected = 0;
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

  device_connected = handle_device_initialization(conf.wait_for_device, preferred_device);
  if (!renderer_initialize(&conf)) {
    SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Failed to initialize renderer.");
    cleanup_resources(device_connected, &conf);
    return EXIT_FAILURE;
  }

#if TARGET_OS_IOS
  // IOS events handler
  SDL_SetEventFilter(handle_app_events, &conf);
#endif

  gamecontrollers_initialize();

#ifndef NDEBUG
  SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);
  SDL_LogDebug(SDL_LOG_CATEGORY_TEST, "Running a Debug build");
#endif

  main_loop(&conf, preferred_device);
  return EXIT_SUCCESS;
}
