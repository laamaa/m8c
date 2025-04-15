//
// Created by jonne on 8/19/24.
//

#ifndef GAMECONTROLLERS_H_
#define GAMECONTROLLERS_H_

#include "config.h"
#include "events.h"

#define MAX_CONTROLLERS 4

int gamecontrollers_initialize();
void gamecontrollers_close();
int gamecontrollers_handle_buttons(const config_params_s *conf);
input_msg_s gamecontrollers_handle_special_messages(const config_params_s *conf);

#endif //GAMECONTROLLERS_H_
