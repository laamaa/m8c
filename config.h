// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef CONFIG_H_
#define CONFIG_H_

#include "ini.h"

typedef struct config_params_s {
  char *filename;
  int init_fullscreen;
} config_params_s;


config_params_s init_config();
void read_config();
int read_fullscreen(ini_t *config);

#endif