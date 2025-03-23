// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

// Contains portions of code from libserialport's examples released to the
// public domain
#ifdef USE_LIBUSB

#include <SDL3/SDL.h>
#include <libusb.h>
#include <stdlib.h>
#include <string.h>

static int ep_out_addr = 0x03;
static int ep_in_addr = 0x83;

#define ACM_CTRL_DTR 0x01
#define ACM_CTRL_RTS 0x02

#define M8_VID 0x16c0
#define M8_PID 0x048a

libusb_context *ctx = NULL;
libusb_device_handle *devh = NULL;

static int do_exit = 0;

int m8_list_devices() {
  int r;
  r = libusb_init(&ctx);
  if (r < 0) {
    SDL_Log("libusb_init failed: %s", libusb_error_name(r));
    return 0;
  }

  libusb_device **device_list = NULL;
  int count = libusb_get_device_list(ctx, &device_list);
  for (size_t idx = 0; idx < count; ++idx) {
    libusb_device *device = device_list[idx];
    struct libusb_device_descriptor desc;
    int rc = libusb_get_device_descriptor(device, &desc);
    if (rc < 0) {
      SDL_Log("Error");
      libusb_free_device_list(device_list, 1);
      return rc;
    }

    if (desc.idVendor == M8_VID && desc.idProduct == M8_PID) {
      SDL_Log("Found M8 device: %d:%d\n", libusb_get_port_number(device),
              libusb_get_bus_number(device));
    }
  }
  libusb_free_device_list(device_list, 1);
  return 0;
}

int usb_loop(void *data) {
  SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);
  while (!do_exit) {
    int rc = libusb_handle_events(ctx);
    if (rc != LIBUSB_SUCCESS) {
      SDL_Log("Audio loop error: %s\n", libusb_error_name(rc));
      break;
    }
  }
  return 0;
}

static SDL_Thread *usb_thread;

static void LIBUSB_CALL xfr_cb_in(struct libusb_transfer *transfer) {
  int *completed = transfer->user_data;
  *completed = 1;
}

int bulk_transfer(int endpoint, uint8_t *serial_buf, int count, unsigned int timeout_ms) {
  if (devh == NULL) {
    return -1;
  }

  int completed = 0;

  struct libusb_transfer *transfer;
  transfer = libusb_alloc_transfer(0);
  libusb_fill_bulk_transfer(transfer, devh, endpoint, serial_buf, count, xfr_cb_in, &completed,
                            timeout_ms);
  int r = libusb_submit_transfer(transfer);

  if (r < 0) {
    SDL_Log("Error");
    libusb_free_transfer(transfer);
    return r;
  }

retry:
  libusb_lock_event_waiters(ctx);
  while (!completed) {
    if (!libusb_event_handler_active(ctx)) {
      libusb_unlock_event_waiters(ctx);
      goto retry;
    }
    libusb_wait_for_event(ctx, NULL);
  }
  libusb_unlock_event_waiters(ctx);

  int actual_length = transfer->actual_length;

  libusb_free_transfer(transfer);

  return actual_length;
}

int blocking_write(void *buf, int count, unsigned int timeout_ms) {
  return bulk_transfer(ep_out_addr, buf, count, timeout_ms);
}

int m8_process_data(uint8_t *serial_buf, int count) {
  return bulk_transfer(ep_in_addr, serial_buf, count, 1);
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
  rc = libusb_control_transfer(devh, 0x21, 0x22, ACM_CTRL_DTR | ACM_CTRL_RTS, 0, NULL, 0, 0);
  if (rc < 0) {
    SDL_Log("Error during control transfer: %s", libusb_error_name(rc));
    return 0;
  }

  /* - set line encoding: here 115200 8N1
   * 115200 = 0x01C200 ~> 0x00, 0xC2, 0x01, 0x00 in little endian
   */
  SDL_Log("Set line encoding");
  unsigned char encoding[] = {0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08};
  rc = libusb_control_transfer(devh, 0x21, 0x20, 0, 0, encoding, sizeof(encoding), 0);
  if (rc < 0) {
    SDL_Log("Error during control transfer: %s", libusb_error_name(rc));
    return 0;
  }

  usb_thread = SDL_CreateThread(&usb_loop, "USB", NULL);

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
  r = libusb_init(&ctx);
  if (r < 0) {
    SDL_Log("libusb_init failed: %s", libusb_error_name(r));
    return 0;
  }
  r = libusb_wrap_sys_device(ctx, (intptr_t)file_descriptor, &devh);
  if (r < 0) {
    SDL_Log("libusb_wrap_sys_device failed: %s", libusb_error_name(r));
    return 0;
  } else if (devh == NULL) {
    SDL_Log("libusb_wrap_sys_device returned invalid handle");
    return 0;
  }
  SDL_Log("USB device init success");

  return init_interface();
}

int m8_initialize(int verbose, char *preferred_device) {

  if (devh != NULL) {
    return 1;
  }

  int r;
  r = libusb_init(&ctx);
  if (r < 0) {
    SDL_Log("libusb_init failed: %s", libusb_error_name(r));
    return 0;
  }
  if (preferred_device == NULL) {
    devh = libusb_open_device_with_vid_pid(ctx, M8_VID, M8_PID);
  } else {
    char *port;
    char *saveptr = NULL;
    char *bus;
    port = SDL_strtok_r(preferred_device, ":", &saveptr);
    bus = SDL_strtok_r(NULL, ":", &saveptr);
    libusb_device **device_list = NULL;
    int count = libusb_get_device_list(ctx, &device_list);
    for (size_t idx = 0; idx < count; ++idx) {
      libusb_device *device = device_list[idx];
      struct libusb_device_descriptor desc;
      r = libusb_get_device_descriptor(device, &desc);
      if (r < 0) {
        SDL_Log("libusb_get_device_descriptor failed: %s", libusb_error_name(r));
        libusb_free_device_list(device_list, 1);
        return 0;
      }

      if (desc.idVendor == M8_VID && desc.idProduct == M8_PID) {
        SDL_Log("Searching for port %s and bus %s", port, bus);
        if (libusb_get_port_number(device) == SDL_atoi(port) &&
            libusb_get_bus_number(device) == SDL_atoi(bus)) {
          SDL_Log("Preferred device found, connecting");
          r = libusb_open(device, &devh);
          if (r < 0) {
            SDL_Log("libusb_open failed: %s", libusb_error_name(r));
            return 0;
          }
        }
      }
    }
    libusb_free_device_list(device_list, 1);
    if (devh == NULL) {
      SDL_Log("Preferred device %s not found, using first available", preferred_device);
      devh = libusb_open_device_with_vid_pid(ctx, M8_VID, M8_PID);
    }
  }
  if (devh == NULL) {
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM,
                 "libusb_open_device_with_vid_pid returned invalid handle");
    return 0;
  }
  SDL_Log("USB device init success");

  return init_interface();
}

int m8_reset_display() {
  int result;

  SDL_Log("Reset display\n");

  char buf[1] = {'R'};

  result = blocking_write(buf, 1, 5);
  if (result != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error resetting M8 display, code %d", result);
    return 0;
  }
  return 1;
}

int m8_enable_and_reset_display() {
  int result;

  SDL_Log("Enabling and resetting M8 display\n");

  char buf[1] = {'E'};
  result = blocking_write(buf, 1, 5);
  if (result != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error enabling M8 display, code %d", result);
    return 0;
  }

  SDL_Delay(5);
  result = m8_reset_display();
  return result;
}

int m8_close() {

  char buf[1] = {'D'};
  int result;

  SDL_Log("Disconnecting M8\n");

  result = blocking_write(buf, 1, 5);
  if (result != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending disconnect, code %d", result);
    return -1;
  }

  int rc;

  if (devh != NULL) {

    for (int if_num = 0; if_num < 2; if_num++) {
      rc = libusb_release_interface(devh, if_num);
      if (rc < 0) {
        SDL_Log("Error releasing interface: %s", libusb_error_name(rc));
        return 0;
      }
    }

    do_exit = 1;

    libusb_close(devh);
  }

  SDL_WaitThread(usb_thread, NULL);

  libusb_exit(ctx);

  return 1;
}

int m8_send_msg_controller(uint8_t input) {
  char buf[2] = {'C', input};
  int nbytes = 2;
  int result;
  result = blocking_write(buf, nbytes, 5);
  if (result != nbytes) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending input, code %d", result);
    return -1;
  }
  return 1;
}

int m8_send_msg_keyjazz(uint8_t note, uint8_t velocity) {
  if (velocity > 0x7F)
    velocity = 0x7F;
  char buf[3] = {'K', note, velocity};
  int nbytes = 3;
  int result;
  result = blocking_write(buf, nbytes, 5);
  if (result != nbytes) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending keyjazz, code %d", result);
    return -1;
  }

  return 1;
}

#endif
