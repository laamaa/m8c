// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef _SERIAL_H_
#define _SERIAL_H_
#ifdef USE_LIBSERIALPORT

#include "../config.h"
#include <stdint.h>

// maximum amount of bytes to read from the serial in one read()
#define serial_read_size 1024

int m8_initialize(int verbose, const char *preferred_device);
int m8_list_devices(void);
int m8_reset_display(void);
int m8_enable_and_reset_display(void);
int m8_send_msg_controller(uint8_t input);
int m8_send_msg_keyjazz(uint8_t note, uint8_t velocity);
int m8_process_data(config_params_s conf);
int m8_close(void);

#endif
#endif
