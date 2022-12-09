// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

/* Uncomment this line to enable debug messages or call make with `make
   CFLAGS=-DDEBUG_MSG` */
// #define DEBUG_MSG

#include <SDL.h>
#include <signal.h>

#include "command.h"
#include "config.h"
#include "input.h"
#include "render.h"
#include "serial.h"
#include "slip.h"

enum state { QUIT, WAIT_FOR_DEVICE, RUN };

enum state run = WAIT_FOR_DEVICE;
uint8_t need_display_reset = 0;

// Handles CTRL+C / SIGINT
void intHandler(int dummy) { run = QUIT; }

void close_serial_port() {
  disconnect();
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

  uint8_t prev_input = 0;
  uint8_t prev_note = 0;
  uint16_t zerobyte_packets = 0; // used to detect device disconnection

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);

  slip_init(&slip, &slip_descriptor);

  // First device detection to avoid SDL init if it isn't necessary
  if (conf.wait_for_device == 0) {
    if (!init_serial(1)) {
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
    int port_inited = init_serial(1);
    if (port_inited) {
      int result;
      result = enable_and_reset_display();
      if (result == 1) {
        run = RUN;
      } else {
        SDL_LogCritical(SDL_LOG_CATEGORY_ERROR,
                        "Device not detected on begin loop.");
        run = QUIT;
      }
    }

    // wait until device is connected
    if (conf.wait_for_device) {
      static uint32_t ticks_poll_device = 0;
      static uint32_t ticks_update_screen = 0;

      if (!port_inited)
        screensaver_init();

      while (run == WAIT_FOR_DEVICE) {
        // get current inputs
        input_msg_s input = get_input_msg(&conf);
        if (input.type == special && input.value == msg_quit) {
          SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Input message QUIT.");
          run = QUIT;
        }

        if (SDL_GetTicks() - ticks_update_screen > 16) {
          ticks_update_screen = SDL_GetTicks();
          screensaver_draw();
          render_screen();
        }

        // Poll for M8 device every second
        if (!port_inited && (SDL_GetTicks() - ticks_poll_device > 1000)) {
          ticks_poll_device = SDL_GetTicks();
          if (run == WAIT_FOR_DEVICE && init_serial(0)) {
            int result = enable_and_reset_display();
            SDL_Delay(100);
            // Device was found; enable display and proceed to the main loop
            if (result == 1) {
              run = RUN;
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
      if (!port_inited) {
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
          send_msg_controller(input.value);
        }
        break;
      case keyjazz:
        if (input.value != 0) {
          if (input.eventType == SDL_KEYDOWN && input.value != prev_input) {
            send_msg_keyjazz(input.value, input.value2);
            prev_note = input.value;
          } else if (input.eventType == SDL_KEYUP && input.value == prev_note) {
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
            SDL_Log("Received msg_quit from input device.");
            run = 0;
            break;
          case msg_reset_display:
            reset_display();
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
            serial_read(serial_buf, serial_read_size);
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
                reset_display();
              } else {
                SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SLIP error %d\n", n);
              }
            }
          }
        } else {
          // zero byte packet, increment counter
          zerobyte_packets++;
          if (zerobyte_packets > conf.wait_packets) {
            zerobyte_packets = 0;

            // try opening the serial port to check if it's alive
            if (check_serial_port()) {
              // the device is still there, carry on
              break;
            } else {
              run = WAIT_FOR_DEVICE;
              close_serial_port();
              /* we'll make one more loop to see if the device is still there
               * but just sending zero bytes. if it doesn't get detected when
               * resetting the port, it will disconnect */
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
  close_serial_port();
  free(serial_buf);
  SDL_Quit();
  return 0;
}
