// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

// Contains portions of code from libserialport's examples released to the
// public domain

#include <SDL_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL_timer.h"
#include <unistd.h>

#include "serial.h"

#ifdef USE_LIBUSB

#include <libusb.h>

static int ep_out_addr = 0x03;
static int ep_in_addr = 0x83;

#define ACM_CTRL_DTR   0x01
#define ACM_CTRL_RTS   0x02

libusb_device_handle *devh = NULL;
int file_descriptor = -1;

void set_file_descriptor(int fd) {
    file_descriptor = fd;
    if (fd == -1) {
        devh = NULL;
    }
}

int blocking_write(void *buf,
                   int count, unsigned int timeout_ms) {
    int actual_length;
    if (libusb_bulk_transfer(devh, ep_out_addr, buf, count,
                             &actual_length, timeout_ms) < 0) {
        SDL_Log("Error while sending char\n");
        return -1;
    }
    return actual_length;
}

int serial_read(uint8_t *serial_buf, int count) {
    int actual_length;
    int rc = libusb_bulk_transfer(devh, ep_in_addr, serial_buf, count, &actual_length,
                                  10);
    if (rc == LIBUSB_ERROR_TIMEOUT) {
        return 0;
    } else if (rc < 0) {
        SDL_Log("Error while waiting for char: %d\n", rc);
        return -1;
    }

    return actual_length;
}

int check_serial_port() {
    libusb_device *device;
    device = libusb_get_device(devh);
    return device != NULL;
}

int handle_from_file_descriptor(int fileDescriptor) {
    libusb_context *ctx = NULL;
    int r;
    r = libusb_set_option(ctx, LIBUSB_OPTION_NO_DEVICE_DISCOVERY, NULL);
    if (r != LIBUSB_SUCCESS) {
        SDL_Log("libusb_init failed: %d\n", r);
        return r;
    }
    r = libusb_init(&ctx);
    if (r < 0) {
        SDL_Log("libusb_init failed: %d\n", r);
        return r;
    }
    r = libusb_wrap_sys_device(ctx, (intptr_t) fileDescriptor, &devh);
    if (r < 0) {
        SDL_Log("libusb_wrap_sys_device failed: %d\n", r);
        return r;
    } else if (devh == NULL) {
        SDL_Log("libusb_wrap_sys_device returned invalid handle\n");
        return r;
    }
    SDL_Log("USB device init success");
    return 0;
}

int init_serial(int verbose) {

    if (devh != NULL) {
        if (verbose)
            SDL_Log("Device already initialised");
        return 1;
    }

    if (file_descriptor == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "File descriptor was not set!\n");
        abort();
    }

    if (verbose)
        SDL_Log("Initialising USB device for %d", file_descriptor);

    int rc;

    rc = handle_from_file_descriptor(file_descriptor);

    if (rc < 0) {
        return rc;
    }

    /* Start configuring the device:
     * - set line state
     */
    SDL_Log("Setting line state");
    rc = libusb_control_transfer(devh, 0x21, 0x22, ACM_CTRL_DTR | ACM_CTRL_RTS,
                                 0, NULL, 0, 0);
    if (rc < 0) {
        SDL_Log("Error during control transfer: %s\n",
                libusb_error_name(rc));
        return 0;
    }

    /* - set line encoding: here 115200 8N1
     * 115200 = 0x01C200 ~> 0x00, 0xC2, 0x01, 0x00 in little endian
     */
    SDL_Log("Set line encoding");
    unsigned char encoding[] = {0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08};
    rc = libusb_control_transfer(devh, 0x21, 0x20, 0, 0, encoding,
                                 sizeof(encoding), 0);
    if (rc < 0) {
        SDL_Log("Error during control transfer: %s\n",
                libusb_error_name(rc));
        return 0;
    }

    return 1;
}

int reset_display() {
    SDL_Log("Reset display\n");
    uint8_t buf[2];
    int result;

    buf[0] = 0x45;
    buf[1] = 0x52;

    result = blocking_write(buf, 2, 5);
    if (result != 2) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error resetting M8 display, code %d",
                     result);
        return 0;
    }
    return 1;
}

int enable_and_reset_display() {
    uint8_t buf[1];
    int result;

    SDL_Log("Enabling and resetting M8 display\n");

    buf[0] = 0x44;
    result = blocking_write(buf, 1, 5);
    if (result != 1) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error enabling M8 display, code %d",
                     result);
        return 0;
    }

    SDL_Delay(5);
    result = reset_display();
    if (result == 1)
        return 1;
    else
        return 0;
}

int disconnect() {
    char buf[1] = {'D'};
    int result;

    SDL_Log("Disconnecting M8\n");

    result = blocking_write(buf, 1, 5);
    if (result != 1) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending disconnect, code %d",
                     result);
        return -1;
    }
    return 1;
}

int send_msg_controller(uint8_t input) {
    char buf[2] = {'C', input};
    int nbytes = 2;
    int result;
    result = blocking_write(buf, nbytes, 5);
    if (result != nbytes) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending input, code %d",
                     result);
        return -1;
    }
    return 1;
}

int send_msg_keyjazz(uint8_t note, uint8_t velocity) {
    if (velocity > 0x7F)
        velocity = 0x7F;
    char buf[3] = {'K', note, velocity};
    int nbytes = 3;
    int result;
    result = blocking_write(buf, nbytes, 5);
    if (result != nbytes) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending keyjazz, code %d",
                     result);
        return -1;
    }

    return 1;
}

#else
#include <libserialport.h>

struct sp_port *m8_port = NULL;

// Helper function for error handling
static int check(enum sp_return result);

static int detect_m8_serial_device(struct sp_port *m8_port) {
  // Check the connection method - we want USB serial devices
  enum sp_transport transport = sp_get_port_transport(m8_port);

  if (transport == SP_TRANSPORT_USB) {
    // Get the USB vendor and product IDs.
    int usb_vid, usb_pid;
    sp_get_port_usb_vid_pid(m8_port, &usb_vid, &usb_pid);

    if (usb_vid == 0x16C0 && usb_pid == 0x048A)
      return 1;
  }

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
      if (strcmp(sp_get_port_name(port), sp_get_port_name(m8_port)) == 0)
        device_found = 1;
    }
  }

  sp_free_port_list(port_list);
  return device_found;

}

int init_serial(int verbose) {
  if(m8_port != NULL) {
    return 1;
  }
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
    if (verbose)
      SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "Cannot find a M8.\n");
  }

  return 1;
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

int reset_display() {
  SDL_Log("Reset display\n");
  uint8_t buf[2];
  int result;

  buf[0] = 0x45;
  buf[1] = 0x52;

  result = sp_blocking_write(m8_port, buf, 2, 5);
  if (result != 2) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error resetting M8 display, code %d",
                 result);
    return 0;
  }
  return 1;
}

int enable_and_reset_display() {
  uint8_t buf[1];
  int result;

  SDL_Log("Enabling and resetting M8 display\n");

  buf[0] = 0x44;
  result = sp_blocking_write(m8_port, buf, 1, 5);
  if (result != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error enabling M8 display, code %d",
                 result);
    return 0;
  }

  SDL_Delay(5);
  result = reset_display();
  if (result == 1)
    return 1;
  else
    return 0;
}

int disconnect() {
  char buf[1] = {'D'};
  int result;

  SDL_Log("Disconnecting M8\n");

  result = sp_blocking_write(m8_port, buf, 1, 5);
  if (result != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending disconnect, code %d",
                 result);
    return -1;
  }
  sp_close(m8_port);
  sp_free_port(m8_port);
  m8_port = NULL;
  return 1;
}

int serial_read(uint8_t *serial_buf, int count) {
  return sp_nonblocking_read(m8_port, serial_buf, count);
}

int send_msg_controller(uint8_t input) {
  char buf[2] = {'C', input};
  size_t nbytes = 2;
  int result;
  result = sp_blocking_write(m8_port, buf, nbytes, 5);
  if (result != nbytes) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending input, code %d",
                 result);
    return -1;
  }
  return 1;
}

int send_msg_keyjazz(uint8_t note, uint8_t velocity) {
  if (velocity > 0x7F)
    velocity = 0x7F;
  char buf[3] = {'K', note, velocity};
  size_t nbytes = 3;
  int result;
  result = sp_blocking_write(m8_port, buf, nbytes, 5);
  if (result != nbytes) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending keyjazz, code %d",
                 result);
    return -1;
  }

  return 1;
}
#endif

