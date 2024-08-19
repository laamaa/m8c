//
// Created by jonne on 8/19/24.
//

#ifndef GAMECONTROLLERS_H_
#define GAMECONTROLLERS_H_

#include "config.h"
#include "input.h"
#include <SDL_gamecontroller.h>

#define MAX_CONTROLLERS 4

int gamecontrollers_initialize();
void gamecontrollers_close();
int gamecontrollers_handle_buttons(config_params_s *conf);
input_msg_s gamecontrollers_handle_special_messages(config_params_s *conf);

#endif //GAMECONTROLLERS_H_
