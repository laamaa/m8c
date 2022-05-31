// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

// Contains portions of code from libserialport's examples released to the
// public domain

#include <SDL_log.h>
#include <libserialport.h>
#include <stdio.h>
#include <stdlib.h>

#include "serial.h"

// Helper function for error handling
static int check(enum sp_return result);

static int detect_m8_serial_device(struct sp_port *port) {
  // Check the connection method - we want USB serial devices
  enum sp_transport transport = sp_get_port_transport(port);

  if (transport == SP_TRANSPORT_USB) {
    // Get the USB vendor and product IDs.
    int usb_vid, usb_pid;
    sp_get_port_usb_vid_pid(port, &usb_vid, &usb_pid);

    if (usb_vid == 0x16C0 && usb_pid == 0x048A)
      return 1;
  }

  return 0;
}

struct sp_port *init_serial(int verbose) {
  /* A pointer to a null-terminated array of pointers to
   * struct sp_port, which will contain the ports found.*/
  struct sp_port *m8_port = NULL;
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
    struct sp_port *port = port_list[i];

    if (detect_m8_serial_device(port)) {
      SDL_Log("Found M8 in %s.\n", sp_get_port_name(port));
      sp_copy_port(port, &m8_port);
    }
  }

  sp_free_port_list(port_list);

  if (m8_port != NULL) {
    // Open the serial port and configure it
    SDL_Log("Opening port.\n");
    enum sp_return result;

    result = sp_open(m8_port, SP_MODE_READ_WRITE);
    if (check(result) != SP_OK)
      return NULL;

    result = sp_set_baudrate(m8_port, 115200);
    if (check(result) != SP_OK)
      return NULL;

    result = sp_set_bits(m8_port, 8);
    if (check(result) != SP_OK)
      return NULL;

    result = sp_set_parity(m8_port, SP_PARITY_NONE);
    if (check(result) != SP_OK)
      return NULL;

    result = sp_set_stopbits(m8_port, 1);
    if (check(result) != SP_OK)
      return NULL;

    result = sp_set_flowcontrol(m8_port, SP_FLOWCONTROL_NONE);
    if (check(result) != SP_OK)
      return NULL;
  } else {
    if (verbose)
      SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "Cannot find a M8.\n");
  }

  return (m8_port);
}

// Helper function for error handling.
static int check(enum sp_return result) {

  char *error_message;

  switch (result) {
  case SP_ERR_ARG:
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error: Invalid argument.\n");
    break;
  case SP_ERR_FAIL:
    error_message = sp_last_error_message();
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error: Failed: %s\n",
                 error_message);
    sp_free_error_message(error_message);
    break;
  case SP_ERR_SUPP:
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error: Not supported.\n");
    break;
  case SP_ERR_MEM:
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Error: Couldn't allocate memory.\n");
    break;
  case SP_OK:
  default:
    break;
  }
  return result;
}
