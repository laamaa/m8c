#ifndef WRITE_H_
#define WRITE_H_

#include <stdint.h>
int enable_and_reset_display(int port);
int disconnect(int port);
int send_msg_controller(int port, uint8_t input);
int send_msg_keyjazz(int port, uint8_t note, uint8_t velocity);


#endif