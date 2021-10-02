// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include <SDL2/SDL.h>
#include <libserialport.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "command.h"
#include "ini.h"
#include "input.h"
#include "render.h"
#include "serial.h"
#include "slip.h"
#include "write.h"

// maximum amount of bytes to read from the serial in one read()
#define serial_read_size 1024

uint8_t run = 1;
uint8_t need_display_reset = 0;

// Handles CTRL+C / SIGINT
void intHandler(int dummy) { run = 0; }

int main(int argc, char *argv[]) {

  // Default initialize fullscreen setting is 0 (false)
  int init_fullscreen = 0;

  // Load the config and read the fullscreen setting from the graphics section
  ini_t *config = ini_load("config.ini");
  const char *setting_fullscren = ini_get(config, "graphics", "fullscreen");
  // Other settings could be read here, too.

  // This obviously requires the parameter to be a lowercase true to enable fullscreen
  if ( strcmp(setting_fullscren, "true") == 0 ) {
    init_fullscreen = 1;
  }

  // Frees the mem used for the config
  ini_free(config);

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

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);

  slip_init(&slip, &slip_descriptor);

  struct sp_port *port;

  port = init_serial();
  if (port == NULL)
    return -1;

  if (enable_and_reset_display(port) == -1)
    run = 0;

  if (initialize_sdl(init_fullscreen) == -1)
    run = 0;

  uint8_t prev_input = 0;

  // main loop
  while (run) {

    // get current inputs
    input_msg_s input = get_input_msg();

    switch (input.type) {
    case normal:
      if (input.value != prev_input) {
        prev_input = input.value;
        send_msg_controller(port, input.value);
      }
      break;
    case keyjazz:
      if (input.value != prev_input) {
        prev_input = input.value;
        if (input.value != 0) {
          send_msg_keyjazz(port, input.value, 0xFF);
        } else {
          send_msg_keyjazz(port, 0, 0);
        }
      }
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

    // read serial port
    size_t bytes_read = sp_nonblocking_read(port, serial_buf, serial_read_size);
    if (bytes_read < 0) {
      SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error %d reading serial. \n",
                      (int)bytes_read);
      run = 0;
    }
    if (bytes_read > 0) {
      for (int i = 0; i < bytes_read; i++) {
        uint8_t rx = serial_buf[i];
        // process the incoming bytes into commands and draw them
        int n = slip_read_byte(&slip, rx);
        if (n != SLIP_NO_ERROR) {
          if (n == SLIP_ERROR_INVALID_PACKET) {
            // Reset display on invalid packets. On current firmwares this can cause a softlock in effect list so this is commented out for now.
            //reset_display(port);
          } else {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SLIP error %d\n", n);
          }
        }
      }
      usleep(10);
    } else {
      render_screen();
      usleep(100);
    }
  }

  // exit, clean up
  SDL_Log("Shutting down\n");
  close_game_controllers();
  close_renderer();
  disconnect(port);
  sp_close(port);
  sp_free_port(port);
  free(serial_buf);

  return 0;
}
