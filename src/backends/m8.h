// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT
#ifndef _M8_H_
#define _M8_H_

#include "../config.h"

enum return_codes {
  DEVICE_DISCONNECTED = 0,
  DEVICE_PROCESSING = 1,
  DEVICE_FATAL_ERROR = -1
};

int m8_initialize(int verbose, const char *preferred_device);
int m8_list_devices(void);
int m8_reset_display(void);
int m8_enable_display(unsigned char reset_display);
int m8_send_msg_controller(unsigned char input);
int m8_send_msg_keyjazz(unsigned char note, unsigned char velocity);
int m8_process_data(const config_params_s *conf);
int m8_pause_processing(void);
int m8_resume_processing(void);
int m8_close(void);

#endif