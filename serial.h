// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef _SERIAL_H_
#define _SERIAL_H_

#ifdef USE_LIBUSB
// Not sure about this value but it seems to work
#define serial_read_size 512
#include <libusb.h>
typedef int (*usb_callback_t)(libusb_device_handle *devh);
void set_usb_init_callback(usb_callback_t callback);
void set_usb_destroy_callback(usb_callback_t callback);
void set_file_descriptor(int fd);
#else
// maximum amount of bytes to read from the serial in one read()
#define serial_read_size 324
#endif

int init_serial(int verbose);
int check_serial_port();
int reset_display();
int enable_and_reset_display();
int disconnect();
int serial_read(uint8_t *serial_buf, int count);
int send_msg_controller(uint8_t input);
int send_msg_keyjazz(uint8_t note, uint8_t velocity);

#endif
