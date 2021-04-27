#ifndef WRITE_H_
#define WRITE_H_

#include <stdint.h>
#include <libserialport.h>

int enable_and_reset_display(struct sp_port *port);
int disconnect(struct sp_port *port);
int send_msg_controller(struct sp_port *port, uint8_t input);
int send_msg_keyjazz(struct sp_port *port, uint8_t note, uint8_t velocity);

#endif