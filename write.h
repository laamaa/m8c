#ifndef WRITE_H_
#define WRITE_H_

#include <stdint.h>
int enable_and_reset_display(int port);
int disconnect(int port);
int send_input(int port, uint8_t input);

void draw_rectangle(struct draw_rectangle_command *command);


#endif