// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef _SERIAL_H_
#define _SERIAL_H_

int init_serial(int verbose);
int check_serial_port();
int reset_display();
int enable_and_reset_display();
int disconnect();
int serial_read(uint8_t *serial_buf, int count);
int send_msg_controller(uint8_t input);
int send_msg_keyjazz(uint8_t note, uint8_t velocity);

#endif
