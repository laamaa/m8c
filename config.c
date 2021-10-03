// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include <SDL2/SDL.h>
#include "config.h"

config_params_s init_config() {
  config_params_s c;

  c.filename        = "config.ini"; // default config file to load
  c.init_fullscreen = 0;            // default fullscreen state at load

  return c;
}

// Read config 
void read_config(struct config_params_s *conf) {
  // Load the config and read the fullscreen setting from the graphics section
  ini_t *ini = ini_load(conf->filename);
  if (ini == NULL) {
    return;
  }

  conf->init_fullscreen = read_fullscreen(ini);

  // Frees the mem used for the config
  ini_free(ini);
}

int read_fullscreen(ini_t *config) {
  const char *param = ini_get(config, "graphics", "fullscreen");
  // This obviously requires the parameter to be a lowercase true to enable fullscreen
  if ( strcmp(param, "true") == 0 ) {
    return 1;
  }
  else return 0;
}