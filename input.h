#ifndef INPUT_H_
#define INPUT_H_

#include <SDL2/SDL_joystick.h>

int initialize_input();
void close_input();
uint8_t process_input();

#endif