// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

// Contains portions of code from libserialport's examples released to the
// public domain
#ifdef USE_LIBUSB

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libusb.h>

#include "serial.h"


static int ep_out_addr = 0x03;
static int ep_in_addr = 0x83;

#define ACM_CTRL_DTR   0x01
#define ACM_CTRL_RTS   0x02

usb_callback_t init_callback = NULL;
usb_callback_t destroy_callback = NULL;
libusb_device_handle *devh = NULL;

void set_usb_init_callback(usb_callback_t callback) {
  init_callback = callback;
}

void set_usb_destroy_callback(usb_callback_t callback) {
  destroy_callback = callback;
}

int blocking_write(void *buf,
 int count, unsigned int timeout_ms) {
  int actual_length;
  int rc;
  rc = libusb_bulk_transfer(devh, ep_out_addr, buf, count,
   &actual_length, timeout_ms);
  if (rc < 0) {
    SDL_Log("Error while sending char: %s", libusb_error_name(rc));
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
    SDL_Log("Error while waiting for char: %s", libusb_error_name(rc));
    return -1;
  }

  return actual_length;
}

int check_serial_port() {
    // Reading will fail anyway when the device is not present anymore
  return 1;
}

int init_interface() {

  if (devh == NULL) {
    SDL_Log("Device not initialised!");
    return 0;
  }

  int rc;

  for (int if_num = 0; if_num < 2; if_num++) {
    if (libusb_kernel_driver_active(devh, if_num)) {
      SDL_Log("Detaching kernel driver for interface %d", if_num);
      libusb_detach_kernel_driver(devh, if_num);
    }
    rc = libusb_claim_interface(devh, if_num);
    if (rc < 0) {
      SDL_Log("Error claiming interface: %s", libusb_error_name(rc));
      return 0;
    }
  }

    /* Start configuring the device:
     * - set line state
     */
  SDL_Log("Setting line state");
  rc = libusb_control_transfer(devh, 0x21, 0x22, ACM_CTRL_DTR | ACM_CTRL_RTS,
   0, NULL, 0, 0);
  if (rc < 0) {
    SDL_Log("Error during control transfer: %s", libusb_error_name(rc));
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
    SDL_Log("Error during control transfer: %s", libusb_error_name(rc));
    return 0;
  }

  return 1;
}

int init_serial_with_file_descriptor(int file_descriptor) {
  SDL_Log("Initialising serial with file descriptor");

  if (file_descriptor <= 0) {
    SDL_Log("Invalid file descriptor: %d", file_descriptor);
    return 0;
  }

  int r;
  r = libusb_set_option(NULL, LIBUSB_OPTION_NO_DEVICE_DISCOVERY, NULL);
  if (r != LIBUSB_SUCCESS) {
    SDL_Log("libusb_set_option failed: %s", libusb_error_name(r));
    return 0;
  }
  r = libusb_init(NULL);
  if (r < 0) {
    SDL_Log("libusb_init failed: %s", libusb_error_name(r));
    return 0;
  }
  r = libusb_wrap_sys_device(NULL, (intptr_t) file_descriptor, &devh);
  if (r < 0) {
    SDL_Log("libusb_wrap_sys_device failed: %s", libusb_error_name(r));
    return 0;
  } else if (devh == NULL) {
    SDL_Log("libusb_wrap_sys_device returned invalid handle");
    return 0;
  }
  SDL_Log("USB device init success");

  if (init_callback != NULL) {
    r = init_callback(devh);

    if (r < 0) {
      SDL_Log("Init callback failed: %d", r);
      return 0;
    }
  }

  return init_interface();
}

int init_serial(int verbose) {

  if (devh != NULL) {
    return 1;
  }

  int r;
  r = libusb_init(NULL);
  if (r < 0) {
    SDL_Log("libusb_init failed: %s", libusb_error_name(r));
    return 0;
  }
  devh = libusb_open_device_with_vid_pid(NULL, 0x16c0, 0x048a);
  if (devh == NULL) {
    SDL_Log("libusb_open_device_with_vid_pid returned invalid handle");
    return 0;
  }
  SDL_Log("USB device init success");

  if (init_callback != NULL) {
    r = init_callback(devh);

    if (r < 0) {
      SDL_Log("Init callback failed: %d", r);
      return 0;
    }
  }

  return init_interface();
}

int reset_display() {
  int result;

  SDL_Log("Reset display\n");

  char buf[1] = {'R'};

  result = blocking_write(buf, 1, 5);
  if (result != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error resetting M8 display, code %d",
     result);
    return 0;
  }
  return 1;
}

int enable_and_reset_display() {
  int result;

  SDL_Log("Enabling and resetting M8 display\n");

  char buf[1] = {'E'};
  result = blocking_write(buf, 1, 5);
  if (result != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error enabling M8 display, code %d",
                 result);
    return 0;
  }

  SDL_Delay(5);
  result = reset_display();
  return result;
}

int disconnect() {

  char buf[1] = {'D'};
  int result;

  if (destroy_callback != NULL) {
    result = destroy_callback(devh);

    if (result < 0) {
      SDL_Log("Destroy callback failed: %d", result);
      return result;
    }
  }

  SDL_Log("Disconnecting M8\n");

  result = blocking_write(buf, 1, 5);
  if (result != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending disconnect, code %d",
     result);
    return -1;
  }

  int rc;

  for (int if_num = 0; if_num < 2; if_num++) {
    rc = libusb_release_interface(devh, if_num);
    if (rc < 0) {
      SDL_Log("Error releasing interface: %s", libusb_error_name(rc));
      return 0;
    }
  }

  if (devh != NULL) {
    libusb_close(devh);
  }

  libusb_exit(NULL);

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

#endif
