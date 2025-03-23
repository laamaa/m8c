// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT
#ifndef _M8_H_
#define _M8_H_

#include "../config.h"

int m8_initialize(int verbose, const char *preferred_device);
int m8_list_devices(void);
int m8_reset_display(void);
int m8_enable_and_reset_display(void);
int m8_send_msg_controller(const unsigned char input);
int m8_send_msg_keyjazz(const unsigned char note, unsigned char velocity);
int m8_process_data(config_params_s conf);
int m8_close(void);

#endif