// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef _EMU_H_
#define _EMU_H_

#include <stdint.h>

int emu_init_config(const char *firmware, const char *sdcard);
int emu_init_serial(int verbose, const char *preferred_device);
int emu_list_devices();
int emu_check_serial_port();
int emu_reset_display();
int emu_enable_and_reset_display();
int emu_disconnect();
int emu_serial_read(uint8_t *serial_buf, int count);
int emu_send_msg_controller(uint8_t input);
int emu_send_msg_keyjazz(uint8_t note, uint8_t velocity);

#endif
