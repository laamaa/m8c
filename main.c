// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include <SDL2/SDL.h>
#include <libserialport.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "command.h"
#include "input.h"
#include "render.h"
#include "serial.h"
#include "slip.h"
#include "write.h"

// maximum amount of bytes to read from the serial in one read()
#define serial_read_size 1024

uint8_t run = 1;

// Handles CTRL+C / SIGINT
void intHandler(int dummy) { run = 0; }

int main(int argc, char *argv[]) {

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

  if (initialize_sdl() == -1)
    run = 0;

  uint8_t prev_input = 0;

  // main loop
  while (run) {

    // read serial port
    size_t bytes_read = sp_nonblocking_read(port, serial_buf, serial_read_size);
    if (bytes_read < 0) {
      fprintf(stderr, "Error %zu reading serial. \n", bytes_read);
      run = 0;
    }
    if (bytes_read > 0) {
      for (int i = 0; i < bytes_read; i++) {
        uint8_t rx = serial_buf[i];
        // process the incoming bytes into commands and draw them
        int n = slip_read_byte(&slip, rx);
        if (n != SLIP_NO_ERROR) {
          fprintf(stderr, "SLIP error %d\n", n);
        }
      }
    } else {
      render_screen();
      usleep(100);
    }

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
      switch (input.value) {
      case msg_quit:
        run = 0;
        break;
      }
      break;
    }
  }

  // exit, clean up
  fprintf(stderr, "\nShutting down\n");
  close_game_controllers();
  close_renderer();
  disconnect(port);
  sp_close(port);
  sp_free_port(port);
  free(serial_buf);

  return 0;
}