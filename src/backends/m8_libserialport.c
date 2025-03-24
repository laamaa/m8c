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
#include "m8.h"
#include "queue.h"
#include "slip.h"

#define SERIAL_READ_SIZE 1024  // maximum amount of bytes to read from the serial in one pass
#define SERIAL_READ_DELAY_MS 4 // delay between serial reads in milliseconds

struct sp_port *m8_port = NULL;
// allocate memory for serial buffers
static uint8_t serial_buffer[SERIAL_READ_SIZE] = {0};
static uint8_t slip_buffer[SERIAL_READ_SIZE] = {0};
static slip_handler_s slip;
message_queue_s queue;

SDL_Thread *serial_thread = NULL;

// Structure to pass data to the thread
typedef struct {
  int should_stop; // Shared stop flag
} thread_params_s;

thread_params_s thread_params;

// Helper function for error handling
static int check(enum sp_return result);

static int send_message_to_queue(uint8_t *data, const uint32_t size) {
  push_message(&queue, data, size);
  return 1;
}

static int disconnect() {
  SDL_Log("Disconnecting M8");

  // wait for serial processing thread to finish
  thread_params.should_stop = 1;
  SDL_WaitThread(serial_thread, NULL);
  destroy_queue(&queue);

  const unsigned char buf[1] = {'D'};

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

static void process_received_bytes(const uint8_t *buffer, int bytes_read, slip_handler_s *slip) {
  const uint8_t *cur = buffer;
  const uint8_t *end = buffer + bytes_read;
  while (cur < end) {
    const int slip_result = slip_read_byte(slip, *cur++);
    if (slip_result != SLIP_NO_ERROR) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SLIP error %d\n", slip_result);
    }
  }
}

static int thread_process_serial_data(void *data) {
  const thread_params_s *thread_params = data;

  while (!thread_params->should_stop) {
    // attempt to read from serial port
    const int bytes_read = sp_nonblocking_read(m8_port, serial_buffer, SERIAL_READ_SIZE);

    if (bytes_read < 0) {
      SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error %d reading serial.", bytes_read);
      disconnect();
      return 0;
    }

    if (bytes_read > 0) {
      process_received_bytes(serial_buffer, bytes_read, &slip);
    }

    SDL_Delay(SERIAL_READ_DELAY_MS);
  }
  return 1;
}

static int configure_serial_port(struct sp_port *port) {
  if (check(sp_open(port, SP_MODE_READ_WRITE)) != SP_OK)
    return 0;
  if (check(sp_set_baudrate(port, 115200)) != SP_OK)
    return 0;
  if (check(sp_set_bits(port, 8)) != SP_OK)
    return 0;
  if (check(sp_set_parity(port, SP_PARITY_NONE)) != SP_OK)
    return 0;
  if (check(sp_set_stopbits(port, 1)) != SP_OK)
    return 0;
  if (check(sp_set_flowcontrol(port, SP_FLOWCONTROL_NONE)) != SP_OK)
    return 0;
  return 1;
}

// Helper function for error handling.
static int check(const enum sp_return result) {
  char *error_message;

  switch (result) {
  case SP_ERR_ARG:
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error: Invalid argument");
    break;
  case SP_ERR_FAIL:
    error_message = sp_last_error_message();
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error: Failed: %s", error_message);
    sp_free_error_message(error_message);
    break;
  case SP_ERR_SUPP:
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error: Not supported");
    break;
  case SP_ERR_MEM:
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error: Couldn't allocate memory");
    break;
  case SP_OK:
  default:
    break;
  }
  return result;
}

// Extracted function for initializing threads and message queue
static int initialize_serial_thread() {

  init_queue(&queue);
  thread_params.should_stop = 0;
  serial_thread = SDL_CreateThread(thread_process_serial_data, "SerialThread", &thread_params);

  if (!serial_thread) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "SDL_CreateThread Error: %s", SDL_GetError());
    SDL_Quit();
    return 0;
  }

  return 1;
}

// Extracted function for detecting and selecting the M8 device
static int find_and_select_device(const char *preferred_device) {
  struct sp_port **port_list;
  const enum sp_return port_result = sp_list_ports(&port_list);

  if (port_result != SP_OK) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "sp_list_ports() failed!");
    return 0;
  }

  for (int i = 0; port_list[i] != NULL; i++) {
    const struct sp_port *port = port_list[i];

    if (detect_m8_serial_device(port)) {
      char *port_name = sp_get_port_name(port);
      SDL_Log("Found M8 in %s", port_name);
      sp_copy_port(port, &m8_port);

      // Break if preferred device is found
      if (preferred_device != NULL && strcmp(preferred_device, port_name) == 0) {
        SDL_Log("Found preferred device, breaking");
        break;
      }
    }
  }

  sp_free_port_list(port_list);
  return (m8_port != NULL);
}

int m8_initialize(const int verbose, const char *preferred_device) {
  if (m8_port != NULL) {
    // Port is already initialized
    return 1;
  }

  // Initialize slip descriptor
  static const slip_descriptor_s slip_descriptor = {
      .buf = slip_buffer,
      .buf_size = sizeof(slip_buffer),
      .recv_message = send_message_to_queue,
  };
  slip_init(&slip, &slip_descriptor);

  if (verbose) {
    SDL_Log("Looking for USB serial devices.\n");
  }

  // Detect and select M8 device
  if (!find_and_select_device(preferred_device)) {
    if (verbose) {
      SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Cannot find a M8");
    }
    return 0;
  }

  // Configure serial port
  if (!configure_serial_port(m8_port)) {
    return 0;
  }

  // Initialize message queue and threads
  return initialize_serial_thread();
}

int m8_send_msg_controller(const uint8_t input) {
  const unsigned char buf[2] = {'C', input};
  const size_t nbytes = 2;
  const int result = sp_blocking_write(m8_port, buf, nbytes, 5);
  if (result != nbytes) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending input, code %d", result);
    return -1;
  }
  return 1;
}

int m8_send_msg_keyjazz(const uint8_t note, uint8_t velocity) {
  if (velocity > 0x7F)
    velocity = 0x7F;
  const unsigned char buf[3] = {'K', note, velocity};
  const size_t nbytes = 3;
  const int result = sp_blocking_write(m8_port, buf, nbytes, 5);
  if (result != nbytes) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending keyjazz, code %d", result);
    return -1;
  }

  return 1;
}

int m8_list_devices() {
  struct sp_port **port_list;
  const enum sp_return result = sp_list_ports(&port_list);

  if (result != SP_OK) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "sp_list_ports() failed!\n");
    return 1;
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

int m8_reset_display() {
  SDL_Log("Reset display\n");

  const unsigned char buf[1] = {'R'};
  const int result = sp_blocking_write(m8_port, buf, 1, 5);
  if (result != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error resetting M8 display, code %d", result);
    return 0;
  }
  return 1;
}

int m8_enable_and_reset_display() {
  SDL_Log("Enabling and resetting M8 display\n");

  const char buf[1] = {'E'};
  int result = sp_blocking_write(m8_port, buf, 1, 5);
  if (result != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error enabling M8 display, code %d", result);
    return 0;
  }

  result = m8_reset_display();

  return result;
}

int m8_process_data(const config_params_s *conf) {
  static unsigned int empty_cycles = 0;

  // Device likely has been disconnected
  if (m8_port == NULL) {
    return DEVICE_DISCONNECTED;
  }

  if (queue_size(&queue) > 0) {
    unsigned char *command;
    empty_cycles = 0;
    size_t length = 0;
    while ((command = pop_message(&queue, &length)) != NULL) {
      if (length > 0) {
        process_command(command, length);
      }
      SDL_free(command);
    }
  } else {
    empty_cycles++;
    if (empty_cycles >= conf->wait_packets) {
      SDL_LogError(SDL_LOG_CATEGORY_SYSTEM,
                   "No messages received for %d cycles, assuming device disconnected",
                   empty_cycles);
      empty_cycles = 0;
      disconnect();
      return DEVICE_DISCONNECTED;
    }
  }
  return DEVICE_PROCESSING;
}

int m8_close() { return disconnect(); }
#endif
