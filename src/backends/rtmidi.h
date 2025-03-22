// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT
#ifndef _MIDI_H_
#define _MIDI_H_
#ifdef USE_RTMIDI

#include "../config.h"

int initialize_rtmidi(void);
int m8_connect(int verbose, const char *preferred_device);
int m8_list_devices(void);
int check_serial_port(void);
int reset_display(void);
int enable_and_reset_display(void);
int disconnect(void);
int send_msg_controller(const unsigned char input);
int send_msg_keyjazz(const unsigned char note, unsigned char velocity);
int process_serial(config_params_s conf);
int m8_close();

#endif
#endif