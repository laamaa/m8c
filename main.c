#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "serial.h"

#include "command.h"
#include "input.h"
#include "render.h"
#include "slip.h"
#include "write.h"

uint8_t run = 1;

// Handles CTRL+C / SIGINT
void intHandler(int dummy) { run = 0; }

int main(int argc, char *argv[]) {

  // maximum amount of bytes to read from the serial in one read()
  const int serial_read_size = 1024;

  // allocate memory for serial buffer
  uint8_t serial_buf[serial_read_size];

  static uint8_t slip_buffer[1024]; // SLIP command buffer

  // settings for the slip packet handler
  static const slip_descriptor_s slip_descriptor = {
      .buf = slip_buffer,
      .buf_size = sizeof(slip_buffer),
      .recv_message = process_command,  //the function where complete slip packets are processed further
  };

  static slip_handler_s slip;

  signal(SIGINT, intHandler);

  slip_init(&slip, &slip_descriptor);

  // open device
  char *portname;
  if (argc > 1) {
    portname = argv[1];
  } else {
    portname = "/dev/ttyACM1";
  }
  int port = init_serial(portname);
  if (port == -1)
    return -1;

  if (enable_and_reset_display(port) == -1)
    run = 0;

  if (initialize_sdl() == -1)
    run = 0;

  // initialize joystick etc.
  initialize_input();

  static uint8_t prev_input = 0;

  // main loop
  while (run) {

    // read data from serial port
    size_t bytes_read = read(port, &serial_buf, sizeof(serial_buf));
    if (bytes_read == -1) {
      fprintf(stderr, "Error %d reading serial: %s\n", errno, strerror(errno));
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
    }

    uint8_t input = process_input();

    if (input != prev_input) {
      prev_input = input;
      switch (input) {
      case 21: // arbitary quit code
        run = 0;
        break;
      default:
        send_input(port, input);
        break;
      }
    } else {
    }

    render_screen();
    
    usleep(10);
  }

  // exit, clean up
  fprintf(stderr, "\nShutting down\n");
  close_input();
  close_renderer();
  disconnect(port);
  close(port);

  return 0;
}