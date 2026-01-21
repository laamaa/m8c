// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

// Contains portions of code from libserialport's examples released to the
// public domain
#include "m8.h"
#ifdef USE_LIBUSB

#include <SDL3/SDL.h>
#include <libusb.h>
#include <stdlib.h>
#include <string.h>
#include "../command.h"
#include "queue.h"
#include "slip.h"

static int ep_out_addr = 0x03;
static int ep_in_addr = 0x83;

#define ACM_CTRL_DTR 0x01
#define ACM_CTRL_RTS 0x02

#define M8_VID 0x16c0
#define M8_PID_STEREO 0x048a
#define M8_PID_MULTICHANNEL 0x048b

#define SERIAL_READ_SIZE 1024  // maximum amount of bytes to read from the serial in one pass

libusb_context *ctx = NULL;
libusb_device_handle *devh = NULL;
static uint8_t serial_buffer[SERIAL_READ_SIZE] = {0};
static uint8_t slip_buffer[SERIAL_READ_SIZE] = {0};
static slip_handler_s slip;
message_queue_s queue;
static int do_exit = 0;
static int async_transfer_active = 0;
static struct libusb_transfer *async_transfer = NULL;
static int shutdown_in_progress = 0;

static int is_m8_device(uint16_t pid) {
  return (pid == M8_PID_STEREO || pid == M8_PID_MULTICHANNEL);
}

static int send_message_to_queue(uint8_t *data, const uint32_t size) {
  push_message(&queue, data, size);
  return 1;
}

int m8_list_devices() {
  int r;
  r = libusb_init(&ctx);
  if (r < 0) {
    SDL_Log("libusb_init failed: %s", libusb_error_name(r));
    return 0;
  }

  libusb_device **device_list = NULL;
  int count = libusb_get_device_list(ctx, &device_list);
  for (size_t idx = 0; idx < (size_t)count; ++idx) {
    libusb_device *device = device_list[idx];
    struct libusb_device_descriptor desc;
    int rc = libusb_get_device_descriptor(device, &desc);
    if (rc < 0) {
      SDL_Log("Error");
      libusb_free_device_list(device_list, 1);
      return rc;
    }

    if (desc.idVendor == M8_VID && is_m8_device(desc.idProduct)) {
      SDL_Log("Found M8 device: %d:%d\n", libusb_get_port_number(device),
              libusb_get_bus_number(device));
    }
  }
  libusb_free_device_list(device_list, 1);
  return 0;
}

static int usb_loop(void *data) {
  (void)data;  // Suppress unused parameter warning
  
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

static int bulk_transfer(int endpoint, uint8_t *serial_buf, int count, unsigned int timeout_ms) {
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

// This function is currently unused but kept for potential future use
__attribute__((unused))
static int bulk_async_transfer(int endpoint, uint8_t *serial_buf, int count, unsigned int timeout_ms,
                        void (*f)(struct libusb_transfer *), void *user_data) {
  struct libusb_transfer *transfer;
  transfer = libusb_alloc_transfer(1);
  libusb_fill_bulk_stream_transfer(transfer, devh, endpoint, 0, serial_buf, count, f, user_data,
                                   timeout_ms);
  int r = libusb_submit_transfer(transfer);

  if (r < 0) {
    SDL_Log("Error");
    libusb_free_transfer(transfer);
    return r;
  }
  return 0;
}

static void async_callback(struct libusb_transfer *xfr) {
  if (shutdown_in_progress) {
    async_transfer_active = 0;
    return;
  }

  if (xfr->status != LIBUSB_TRANSFER_COMPLETED) {
    if (xfr->status == LIBUSB_TRANSFER_CANCELLED) {
      SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Async transfer cancelled");
      async_transfer_active = 0;
      return;
    }
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Async transfer failed with status: %s", 
                 libusb_error_name(xfr->status));
    if (!shutdown_in_progress && libusb_submit_transfer(xfr) < 0) {
      SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error re-submitting failed transfer");
      async_transfer_active = 0;
    }
    return;
  }

  int bytes_read = xfr->actual_length;
  if (bytes_read < 0) {
    SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error %d reading serial", (int)bytes_read);
  } else if (bytes_read > 0) {
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Received %d bytes from M8", bytes_read);
    uint8_t *serial_buf = xfr->buffer;
    uint8_t *cur = serial_buf;
    const uint8_t *end = serial_buf + bytes_read;
    slip_handler_s *slip = (slip_handler_s *)xfr->user_data;
    while (cur < end) {
      // process the incoming bytes into commands and draw them
      int n = slip_read_byte(slip, *(cur++));
      if (n != SLIP_NO_ERROR) {
        if (n == SLIP_ERROR_INVALID_PACKET) {
          SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Invalid SLIP packet!\n");

        } else {
          SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SLIP error %d\n", n);
        }
      }
    }
  }
  if (!shutdown_in_progress) {
    int submit_result = libusb_submit_transfer(xfr);
    if (submit_result < 0) {
      SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error re-submitting URB: %s", 
                   libusb_error_name(submit_result));
      async_transfer_active = 0;
    }
  } else {
    async_transfer_active = 0;
  }
}

int async_read_start(uint8_t *serial_buf, int count, slip_handler_s *slip) {
  if (async_transfer_active) {
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Async transfer already active, skipping");
    return 0; // Already active
  }
  
  if (devh == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Device handle is NULL, cannot start async transfer");
    return -1;
  }
  
  async_transfer = libusb_alloc_transfer(1);
  if (!async_transfer) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to allocate async transfer");
    return -1;
  }
  
  libusb_fill_bulk_stream_transfer(async_transfer, devh, ep_in_addr, 0, serial_buf, count, 
                                   &async_callback, slip, 300);
  int r = libusb_submit_transfer(async_transfer);
  
  if (r < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error starting async transfer: %s", libusb_error_name(r));
    libusb_free_transfer(async_transfer);
    async_transfer = NULL;
    return r;
  }
  
  async_transfer_active = 1;
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Async transfer started successfully");
  return 0;
}

void async_read_stop() {
  shutdown_in_progress = 1;
  
  if (async_transfer_active && async_transfer) {
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Stopping async transfer");
    int cancel_result = libusb_cancel_transfer(async_transfer);
    if (cancel_result < 0) {
      if (cancel_result == LIBUSB_ERROR_NOT_FOUND) {
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Transfer already completed or cancelled");
      } else if (cancel_result == LIBUSB_ERROR_INVALID_PARAM) {
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Transfer not valid for cancellation");
      } else {
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Transfer cancellation returned: %s", 
                     libusb_error_name(cancel_result));
      }
    }
    // Wait briefly for the callback to complete
    for (int i = 0; i < 10 && async_transfer_active; i++) {
      SDL_Delay(1);
    }
    // Force cleanup if still active
    async_transfer_active = 0;
  }
}

int m8_process_data(const config_params_s *conf) {
  (void)conf; // Suppress unused parameter warning
  static message_batch_s batch;

  // Process any queued messages
  if (pop_all_messages(&queue, &batch) > 0) {
    for (unsigned int i = 0; i < batch.count; i++) {
      if (batch.lengths[i] > 0) {
        process_command(batch.messages[i], batch.lengths[i]);
      }
      SDL_free(batch.messages[i]);
    }
  }
  return DEVICE_PROCESSING;
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
      libusb_close(devh);
      devh = NULL;
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

  init_queue(&queue);

  usb_thread = SDL_CreateThread(&usb_loop, "USB", NULL);

  // Start async transfer for reading data from M8
  if (async_read_start(serial_buffer, SERIAL_READ_SIZE, &slip) < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to start async transfer during initialization");
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

int m8_initialize(int verbose, const char *preferred_device) {
  (void)verbose; // Suppress unused parameter warning
  (void)preferred_device; // Suppress unused parameter warning

  if (devh != NULL) {
    return 1;
  }

  // Initialize slip descriptor
  static const slip_descriptor_s slip_descriptor = {
      .buf = slip_buffer,
      .buf_size = sizeof(slip_buffer),
      .recv_message = send_message_to_queue,
  };
  slip_init(&slip, &slip_descriptor);

  int r;
  r = libusb_init(&ctx);
  if (r < 0) {
    SDL_Log("libusb_init failed: %s", libusb_error_name(r));
    return 0;
  }
  if (preferred_device == NULL) {
    devh = libusb_open_device_with_vid_pid(ctx, M8_VID, M8_PID_STEREO);
    if (devh == NULL) {
      devh = libusb_open_device_with_vid_pid(ctx, M8_VID, M8_PID_MULTICHANNEL);
    }
  } else {
    char *port;
    char *saveptr = NULL;
    char *bus;
    char *device_copy = SDL_strdup(preferred_device);  // Create a copy to avoid const qualifier warning
    if (device_copy == NULL) {
      SDL_Log("Failed to allocate memory for device string");
      return 0;
    }
    port = SDL_strtok_r(device_copy, ":", &saveptr);
    bus = SDL_strtok_r(NULL, ":", &saveptr);
    libusb_device **device_list = NULL;
    int count = libusb_get_device_list(ctx, &device_list);
    for (size_t idx = 0; idx < (size_t)count; ++idx) {
      libusb_device *device = device_list[idx];
      struct libusb_device_descriptor desc;
      r = libusb_get_device_descriptor(device, &desc);
      if (r < 0) {
        SDL_Log("libusb_get_device_descriptor failed: %s", libusb_error_name(r));
        libusb_free_device_list(device_list, 1);
        SDL_free(device_copy);
        return 0;
      }

      if (desc.idVendor == M8_VID && is_m8_device(desc.idProduct)) {
        SDL_Log("Searching for port %s and bus %s", port, bus);
        if (libusb_get_port_number(device) == SDL_atoi(port) &&
            libusb_get_bus_number(device) == SDL_atoi(bus)) {
          SDL_Log("Preferred device found, connecting");
          r = libusb_open(device, &devh);
          if (r < 0) {
            SDL_Log("libusb_open failed: %s", libusb_error_name(r));
            libusb_free_device_list(device_list, 1);
            SDL_free(device_copy);
            return 0;
          }
        }
      }
    }
    libusb_free_device_list(device_list, 1);
    SDL_free(device_copy);  // Free the allocated copy
    if (devh == NULL) {
      SDL_Log("Preferred device %s not found, using first available", preferred_device);
      devh = libusb_open_device_with_vid_pid(ctx, M8_VID, M8_PID_STEREO);
      if (devh == NULL) {
        devh = libusb_open_device_with_vid_pid(ctx, M8_VID, M8_PID_MULTICHANNEL);
      }
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

int m8_enable_display(const unsigned char reset_display) {
  (void)reset_display; // Suppress unused parameter warning
  if (devh == NULL) {
    return 0;
  }

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

  // Stop async transfer first
  async_read_stop();

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

  destroy_queue(&queue);
  
  if (async_transfer) {
    libusb_free_transfer(async_transfer);
    async_transfer = NULL;
  }
  async_transfer_active = 0;
  shutdown_in_progress = 0;

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

// These shouldn't be needed with serial
int m8_pause_processing(void) { return 1; }
int m8_resume_processing(void) { return 1; }

#endif
