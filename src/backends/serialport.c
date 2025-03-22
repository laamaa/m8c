// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

// Contains portions of code from libserialport's examples released to the
// public domain

#ifdef USE_LIBSERIALPORT
#include <SDL3/SDL.h>
#include <libserialport.h>
#include <stdlib.h>
#include <string.h>

#include "../command.h"
#include "../config.h"
#include "serialport.h"
#include "slip.h"

struct sp_port *m8_port = NULL;
// allocate memory for serial buffers
static uint8_t serial_buffer[serial_read_size] = {0};
static uint8_t slip_buffer[serial_read_size] = {0};
static slip_handler_s slip;
static uint16_t zero_byte_packets = 0; // used to detect device disconnection

// Helper function for error handling
static int check(enum sp_return result);

static int detect_m8_serial_device(const struct sp_port *m8_port) {
  // Check the connection method - we want USB serial devices
  const enum sp_transport transport = sp_get_port_transport(m8_port);

  if (transport == SP_TRANSPORT_USB) {
    // Get the USB vendor and product IDs.
    int usb_vid, usb_pid;
    sp_get_port_usb_vid_pid(m8_port, &usb_vid, &usb_pid);

    if (usb_vid == 0x16C0 && usb_pid == 0x048A)
      return 1;
  }

  return 0;
}

int m8_list_devices() {
  struct sp_port **port_list;
  const enum sp_return result = sp_list_ports(&port_list);

  if (result != SP_OK) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "sp_list_ports() failed!\n");
    abort();
  }

  for (int i = 0; port_list[i] != NULL; i++) {
    const struct sp_port *port = port_list[i];

    if (detect_m8_serial_device(port)) {
      SDL_Log("Found M8 device: %s", sp_get_port_name(port));
    }
  }

  sp_free_port_list(port_list);
  return 0;
}

// Checks for connected devices and whether the specified device still exists
int check_serial_port() {

  int device_found = 0;

  /* A pointer to a null-terminated array of pointers to
   * struct sp_port, which will contain the ports found.*/
  struct sp_port **port_list;

  /* Call sp_list_ports() to get the ports. The port_list
   * pointer will be updated to refer to the array created. */
  const enum sp_return result = sp_list_ports(&port_list);

  if (result != SP_OK) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "sp_list_ports() failed!\n");
    abort();
  }

  /* Iterate through the ports. When port_list[i] is NULL
   * this indicates the end of the list. */
  for (int i = 0; port_list[i] != NULL; i++) {
    const struct sp_port *port = port_list[i];

    if (detect_m8_serial_device(port)) {
      if (strcmp(sp_get_port_name(port), sp_get_port_name(m8_port)) == 0)
        device_found = 1;
    }
  }

  sp_free_port_list(port_list);
  return device_found;
}

int m8_connect(const int verbose, const char *preferred_device) {
  if (m8_port != NULL) {
    // Port is already initialized
    return 1;
  }

  // settings for the slip packet handler
  static const slip_descriptor_s slip_descriptor = {
      .buf = slip_buffer,
      .buf_size = sizeof(slip_buffer),
      .recv_message = process_command, // the function where complete slip
                                       // packets are processed further
  };

  slip_init(&slip, &slip_descriptor);

  /* A pointer to a null-terminated array of pointers to
   * struct sp_port, which will contain the ports found.*/
  struct sp_port **port_list;

  if (verbose)
    SDL_Log("Looking for USB serial devices.\n");

  /* Call sp_list_ports() to get the ports. The port_list
   * pointer will be updated to refer to the array created. */
  enum sp_return result = sp_list_ports(&port_list);

  if (result != SP_OK) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "sp_list_ports() failed!\n");
    abort();
  }

  /* Iterate through the ports. When port_list[i] is NULL
   * this indicates the end of the list. */
  for (int i = 0; port_list[i] != NULL; i++) {
    const struct sp_port *port = port_list[i];

    if (detect_m8_serial_device(port)) {
      char *port_name = sp_get_port_name(port);
      SDL_Log("Found M8 in %s.\n", port_name);
      sp_copy_port(port, &m8_port);
      if (preferred_device != NULL && strcmp(preferred_device, port_name) == 0) {
        SDL_Log("Found preferred device, breaking");
        break;
      }
    }
  }

  sp_free_port_list(port_list);

  if (m8_port != NULL) {
    // Open the serial port and configure it
    SDL_Log("Opening port.");

    result = sp_open(m8_port, SP_MODE_READ_WRITE);
    if (check(result) != SP_OK)
      return 0;

    result = sp_set_baudrate(m8_port, 115200);
    if (check(result) != SP_OK)
      return 0;

    result = sp_set_bits(m8_port, 8);
    if (check(result) != SP_OK)
      return 0;

    result = sp_set_parity(m8_port, SP_PARITY_NONE);
    if (check(result) != SP_OK)
      return 0;

    result = sp_set_stopbits(m8_port, 1);
    if (check(result) != SP_OK)
      return 0;

    result = sp_set_flowcontrol(m8_port, SP_FLOWCONTROL_NONE);
    if (check(result) != SP_OK)
      return 0;
  } else {
    if (verbose) {
      SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "Cannot find a M8.\n");
    }
    return 0;
  }

  return 1;
}

// Helper function for error handling.
static int check(const enum sp_return result) {

  char *error_message;

  switch (result) {
  case SP_ERR_ARG:
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error: Invalid argument.\n");
    break;
  case SP_ERR_FAIL:
    error_message = sp_last_error_message();
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error: Failed: %s\n", error_message);
    sp_free_error_message(error_message);
    break;
  case SP_ERR_SUPP:
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error: Not supported.\n");
    break;
  case SP_ERR_MEM:
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error: Couldn't allocate memory.\n");
    break;
  case SP_OK:
  default:
    break;
  }
  return result;
}

int reset_display() {
  SDL_Log("Reset display\n");

  const char buf[1] = {'R'};
  const int result = sp_blocking_write(m8_port, buf, 1, 5);
  if (result != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error resetting M8 display, code %d", result);
    return 0;
  }
  return 1;
}

int enable_and_reset_display() {

  SDL_Log("Enabling and resetting M8 display\n");

  const char buf[1] = {'E'};
  int result = sp_blocking_write(m8_port, buf, 1, 5);
  if (result != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error enabling M8 display, code %d", result);
    return 0;
  }

  result = reset_display();

  return result;
}

int disconnect() {

  SDL_Log("Disconnecting M8\n");

  const char buf[1] = {'D'};

  int result = sp_blocking_write(m8_port, buf, 1, 5);
  if (result != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending disconnect, code %d", result);
    result = 0;
  }
  sp_close(m8_port);
  sp_free_port(m8_port);
  m8_port = NULL;
  return result;
}

int serial_read(uint8_t *serial_buf, const int count) {
  return sp_nonblocking_read(m8_port, serial_buf, count);
}

int send_msg_controller(const uint8_t input) {
  const char buf[2] = {'C', input};
  const size_t nbytes = 2;
  const int result = sp_blocking_write(m8_port, buf, nbytes, 5);
  if (result != nbytes) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending input, code %d", result);
    return -1;
  }
  return 1;
}

int send_msg_keyjazz(const uint8_t note, uint8_t velocity) {
  if (velocity > 0x7F)
    velocity = 0x7F;
  const char buf[3] = {'K', note, velocity};
  const size_t nbytes = 3;
  const int result = sp_blocking_write(m8_port, buf, nbytes, 5);
  if (result != nbytes) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending keyjazz, code %d", result);
    return -1;
  }

  return 1;
}

int process_serial(config_params_s conf) {
  while (1) {
    // read serial port
    const int bytes_read = serial_read(serial_buffer, serial_read_size);
    if (bytes_read < 0) {
      SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error %d reading serial.", bytes_read);
      disconnect();
      return -1;
      break;
    }
    if (bytes_read > 0) {
      // input from device: reset the zero byte counter and create a
      // pointer to the serial buffer
      zero_byte_packets = 0;
      const uint8_t *cur = serial_buffer;
      const uint8_t *end = serial_buffer + bytes_read;
      while (cur < end) {
        // process the incoming bytes into commands and draw them
        const int n = slip_read_byte(&slip, *cur++);
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
      zero_byte_packets++;
      if (zero_byte_packets > conf.wait_packets) {
        zero_byte_packets = 0;

        // try opening the serial port to check if it's alive
        if (check_serial_port()) {
          // the device is still there, carry on
          break;
        }
        disconnect();
        return 0;
      }
      break;
    }
  }
  return 1;
}

int m8_close() {
  return disconnect();
}
#endif
