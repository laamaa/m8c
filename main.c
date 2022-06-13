// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

/* Uncomment this line to enable debug messages or call make with `make
   CFLAGS=-DDEBUG_MSG` */
// #define DEBUG_MSG

#include <SDL.h>
#include <libserialport.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "command.h"
#include "config.h"
#include "input.h"
#include "render.h"
#include "serial.h"
#include "slip.h"
#include "write.h"

// maximum amount of bytes to read from the serial in one read()
#define serial_read_size 324

enum state { QUIT, WAIT_FOR_DEVICE, RUN };

enum state run = WAIT_FOR_DEVICE;
uint8_t need_display_reset = 0;

// Handles CTRL+C / SIGINT
void intHandler(int dummy) { run = QUIT; }

void close_serial_port(struct sp_port *port) {
  disconnect(port);
  sp_close(port);
  sp_free_port(port);
}

int main(int argc, char *argv[]) {
  // Initialize the config to defaults read in the params from the
  // configfile if present
  config_params_s conf = init_config();

  // TODO: take cli parameter to override default configfile location
  read_config(&conf);

  // allocate memory for serial buffer
  uint8_t *serial_buf = malloc(serial_read_size);

  static uint8_t slip_buffer[serial_read_size]; // SLIP command buffer

  // settings for the slip packet handler
  static const slip_descriptor_s slip_descriptor = {
      .buf = slip_buffer,
      .buf_size = sizeof(slip_buffer),
      .recv_message = process_command, // the function where complete slip
                                       // packets are processed further
  };

  static slip_handler_s slip;
  struct sp_port *port = NULL;

  uint8_t prev_input = 0;
  uint8_t prev_note = 0;
  uint16_t zerobyte_packets = 0; // used to detect device disconnection

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);

  slip_init(&slip, &slip_descriptor);

  // First device detection to avoid SDL init if it isn't necessary
  if (conf.wait_for_device == 0) {
    port = init_serial(1);
    if (port == NULL) {
      free(serial_buf);
      return -1;
    }
  }

  // initialize all SDL systems
  if (initialize_sdl(conf.init_fullscreen, conf.init_use_gpu) == -1)
    run = QUIT;

  // initial scan for (existing) game controllers
  initialize_game_controllers();

#ifdef DEBUG_MSG
  SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
#endif

  // main loop begin
  do {
    if (port == NULL)
      port = init_serial(1);
    if (port != NULL) {
      int result;
      result = enable_and_reset_display(port);
      if (result == 1) {
        run = RUN;
      } else {
        run = QUIT;
      }
    }

    // wait until device is connected
    if (conf.wait_for_device) {
      static uint32_t ticks_poll_device = 0;
      static uint32_t ticks_update_screen = 0;

      if (port == NULL)
        screensaver_init();

      while (run == WAIT_FOR_DEVICE) {
        // get current inputs
        input_msg_s input = get_input_msg(&conf);
        if (input.type == special && input.value == msg_quit) {
          run = QUIT;
        }

        if (SDL_GetTicks() - ticks_update_screen > 16) {
          ticks_update_screen = SDL_GetTicks();
          screensaver_draw();
          render_screen();
        }

        // Poll for M8 device every second
        if (!port && (SDL_GetTicks() - ticks_poll_device > 1000)) {
          ticks_poll_device = SDL_GetTicks();
          port = init_serial(0);
          if (run == WAIT_FOR_DEVICE && port != NULL) {
            int result = enable_and_reset_display(port);
            SDL_Delay(100);
            // Device was found; enable display and proceed to the main loop
            if (result == 1) {
              run = RUN;
              screensaver_destroy();
            } else {
              run = QUIT;
              screensaver_destroy();
            }
          }
        }

        SDL_Delay(conf.idle_ms);
      }

    } else {
      // classic startup behaviour, exit if device is not found
      if (port == NULL) {
        close_game_controllers();
        close_renderer();
        SDL_Quit();
        free(serial_buf);
        return -1;
      }
    }

    // main loop
    while (run == RUN) {

      // get current inputs
      input_msg_s input = get_input_msg(&conf);

      switch (input.type) {
      case normal:
        if (input.value != prev_input) {
          prev_input = input.value;
          send_msg_controller(port, input.value);
        }
        break;
      case keyjazz:
        if (input.value != 0) {
          if (input.eventType == SDL_KEYDOWN && input.value != prev_input) {
            send_msg_keyjazz(port, input.value, input.value2);
            prev_note = input.value;
          } else if (input.eventType == SDL_KEYUP && input.value == prev_note) {
            send_msg_keyjazz(port, 0xFF, 0);
          }
        }
        prev_input = input.value;
        break;
      case special:
        if (input.value != prev_input) {
          prev_input = input.value;
          switch (input.value) {
          case msg_quit:
            run = 0;
            break;
          case msg_reset_display:
            reset_display(port);
            break;
          default:
            break;
          }
          break;
        }
      }

      while (1) {
        // read serial port
        int bytes_read =
            sp_nonblocking_read(port, serial_buf, serial_read_size);
        if (bytes_read < 0) {
          SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error %d reading serial. \n",
                          (int)bytes_read);
          run = QUIT;
          break;
        } else if (bytes_read > 0) {
          // input from device: reset the zero byte counter and create a
          // pointer to the serial buffer
          zerobyte_packets = 0;
          uint8_t *cur = serial_buf;
          const uint8_t *end = serial_buf + bytes_read;
          while (cur < end) {
            // process the incoming bytes into commands and draw them
            int n = slip_read_byte(&slip, *(cur++));
            if (n != SLIP_NO_ERROR) {
              if (n == SLIP_ERROR_INVALID_PACKET) {
                reset_display(port);
              } else {
                SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SLIP error %d\n", n);
              }
            }
          }
        } else {
          // zero byte packet, increment counter
          zerobyte_packets++;
          if (zerobyte_packets > 128) {
            // i guess it can be assumed that the device has been disconnected
            if (conf.wait_for_device) {
              run = WAIT_FOR_DEVICE;
              close_serial_port(port);
              port = NULL;
              break;
            } else {
              run = QUIT;
            }
          }
          break;
        }
      }
      render_screen();
      SDL_Delay(conf.idle_ms);
    }
  } while (run > QUIT);
  // main loop end

  // exit, clean up
  SDL_Log("Shutting down\n");
  close_game_controllers();
  close_renderer();
  close_serial_port(port);
  free(serial_buf);
  SDL_Quit();
  return 0;
}
