// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include <SDL2/SDL.h>
#include "config.h"

config_params_s init_config() {
  config_params_s c;

  c.filename        = "config.ini"; // default config file to load
  c.init_fullscreen = 0;            // default fullscreen state at load

  c.key_up          = SDL_SCANCODE_UP;
  c.key_left        = SDL_SCANCODE_LEFT;
  c.key_down        = SDL_SCANCODE_DOWN;
  c.key_right       = SDL_SCANCODE_RIGHT;
  c.key_select      = SDL_SCANCODE_LSHIFT;
  c.key_select_alt  = SDL_SCANCODE_A;
  c.key_start       = SDL_SCANCODE_SPACE;
  c.key_start_alt   = SDL_SCANCODE_S;
  c.key_opt         = SDL_SCANCODE_LALT;
  c.key_opt_alt     = SDL_SCANCODE_Z;
  c.key_edit        = SDL_SCANCODE_LCTRL;
  c.key_edit_alt    = SDL_SCANCODE_X;
  c.key_delete      = SDL_SCANCODE_DELETE;
  c.key_reset       = SDL_SCANCODE_R;
  c.key_gl_shader   = SDL_SCANCODE_F11;
  c.key_visualizer  = SDL_SCANCODE_F12;

  c.gamepad_up                    = SDL_CONTROLLER_BUTTON_DPAD_UP;
  c.gamepad_left                  = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
  c.gamepad_down                  = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
  c.gamepad_right                 = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
  c.gamepad_select                = SDL_CONTROLLER_BUTTON_BACK;
  c.gamepad_start                 = SDL_CONTROLLER_BUTTON_START;
  c.gamepad_opt                   = SDL_CONTROLLER_BUTTON_B;
  c.gamepad_edit                  = SDL_CONTROLLER_BUTTON_A;

  c.gamepad_analog_threshold      = 32767;
  c.gamepad_analog_invert         = 0;
  c.gamepad_analog_axis_updown    = SDL_CONTROLLER_AXIS_LEFTY;
  c.gamepad_analog_axis_leftright = SDL_CONTROLLER_AXIS_LEFTX;
  c.gamepad_analog_axis_start     = SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
  c.gamepad_analog_axis_select    = SDL_CONTROLLER_AXIS_TRIGGERLEFT;
  c.gamepad_analog_axis_opt       = SDL_CONTROLLER_AXIS_INVALID;
  c.gamepad_analog_axis_edit      = SDL_CONTROLLER_AXIS_INVALID;


  return c;
}

// Read config 
void read_config(config_params_s *conf) {
  // Load the config and read the fullscreen setting from the graphics section
  ini_t *ini = ini_load(conf->filename);
  if (ini == NULL) {
    return;
  }

  read_graphics_config(ini, conf);
  read_key_config(ini, conf);
  read_gamepad_config(ini, conf);

  // Frees the mem used for the config
  ini_free(ini);
}

void read_graphics_config(ini_t *ini, config_params_s *conf) {
  const char *param = ini_get(ini, "graphics", "fullscreen");
  // This obviously requires the parameter to be a lowercase true to enable fullscreen
  if ( strcmp(param, "true") == 0 ) {
    conf->init_fullscreen = 1;
  }
  else conf->init_fullscreen = 0;
}

void read_key_config(ini_t *ini, config_params_s *conf) {
  // TODO: Some form of validation

  const char *key_up          = ini_get(ini, "keyboard", "key_up");
  const char *key_left        = ini_get(ini, "keyboard", "key_left");
  const char *key_down        = ini_get(ini, "keyboard", "key_down");
  const char *key_right       = ini_get(ini, "keyboard", "key_right");
  const char *key_select      = ini_get(ini, "keyboard", "key_select");
  const char *key_select_alt  = ini_get(ini, "keyboard", "key_select_alt");
  const char *key_start       = ini_get(ini, "keyboard", "key_start");
  const char *key_start_alt   = ini_get(ini, "keyboard", "key_start_alt");
  const char *key_opt         = ini_get(ini, "keyboard", "key_opt");
  const char *key_opt_alt     = ini_get(ini, "keyboard", "key_opt_alt");
  const char *key_edit        = ini_get(ini, "keyboard", "key_edit");
  const char *key_edit_alt    = ini_get(ini, "keyboard", "key_edit_alt");
  const char *key_delete      = ini_get(ini, "keyboard", "key_delete");
  const char *key_reset       = ini_get(ini, "keyboard", "key_reset");
  const char *key_gl_shader   = ini_get(ini, "keyboard", "key_gl_shader");
  const char *key_visualizer   = ini_get(ini, "keyboard", "key_visualizer");

  conf->key_up          = atoi(key_up);
  conf->key_left        = atoi(key_left);
  conf->key_down        = atoi(key_down);
  conf->key_right       = atoi(key_right);
  conf->key_select      = atoi(key_select);
  conf->key_select_alt  = atoi(key_select_alt);
  conf->key_start       = atoi(key_start);
  conf->key_start_alt   = atoi(key_start_alt);
  conf->key_opt         = atoi(key_opt);
  conf->key_opt_alt     = atoi(key_opt_alt);
  conf->key_edit        = atoi(key_edit);
  conf->key_edit_alt    = atoi(key_edit_alt);
  conf->key_delete      = atoi(key_delete);
  conf->key_reset       = atoi(key_reset);
  conf->key_gl_shader   = atoi(key_gl_shader);
  conf->key_visualizer  = atoi(key_visualizer);
}

void read_gamepad_config(ini_t *ini, config_params_s *conf) {
  // TODO: Some form of validation

  const char *gamepad_up                    = ini_get(ini, "gamepad", "gamepad_up");
  const char *gamepad_left                  = ini_get(ini, "gamepad", "gamepad_left");
  const char *gamepad_down                  = ini_get(ini, "gamepad", "gamepad_down");
  const char *gamepad_right                 = ini_get(ini, "gamepad", "gamepad_right");
  const char *gamepad_select                = ini_get(ini, "gamepad", "gamepad_select");
  const char *gamepad_start                 = ini_get(ini, "gamepad", "gamepad_start");
  const char *gamepad_opt                   = ini_get(ini, "gamepad", "gamepad_opt");
  const char *gamepad_edit                  = ini_get(ini, "gamepad", "gamepad_edit");
  const char *gamepad_analog_threshold      = ini_get(ini, "gamepad", "gamepad_analog_threshold");
  const char *gamepad_analog_invert         = ini_get(ini, "gamepad", "gamepad_analog_invert");
  const char *gamepad_analog_axis_updown    = ini_get(ini, "gamepad", "gamepad_analog_axis_updown");
  const char *gamepad_analog_axis_leftright = ini_get(ini, "gamepad", "gamepad_analog_axis_leftright");
  const char *gamepad_analog_axis_select    = ini_get(ini, "gamepad", "gamepad_analog_axis_select");
  const char *gamepad_analog_axis_start     = ini_get(ini, "gamepad", "gamepad_analog_axis_start");
  const char *gamepad_analog_axis_opt       = ini_get(ini, "gamepad", "gamepad_analog_axis_opt");
  const char *gamepad_analog_axis_edit      = ini_get(ini, "gamepad", "gamepad_analog_axis_edit");

  conf->gamepad_up                    = atoi(gamepad_up);
  conf->gamepad_left                  = atoi(gamepad_left);
  conf->gamepad_down                  = atoi(gamepad_down);
  conf->gamepad_right                 = atoi(gamepad_right);
  conf->gamepad_select                = atoi(gamepad_select);
  conf->gamepad_start                 = atoi(gamepad_start);
  conf->gamepad_opt                   = atoi(gamepad_opt);
  conf->gamepad_edit                  = atoi(gamepad_edit);
  conf->gamepad_analog_threshold      = atoi(gamepad_analog_threshold);

  // This obviously requires the parameter to be a lowercase true to enable fullscreen
  if ( strcmp(gamepad_analog_invert, "true") == 0 ) {
    conf->gamepad_analog_invert = 1;
  }
  else conf->gamepad_analog_invert = 0;

  conf->gamepad_analog_axis_updown    = atoi(gamepad_analog_axis_updown);
  conf->gamepad_analog_axis_leftright = atoi(gamepad_analog_axis_leftright);
  conf->gamepad_analog_axis_select    = atoi(gamepad_analog_axis_select);
  conf->gamepad_analog_axis_start     = atoi(gamepad_analog_axis_start);
  conf->gamepad_analog_axis_opt       = atoi(gamepad_analog_axis_opt);
  conf->gamepad_analog_axis_edit      = atoi(gamepad_analog_axis_edit);  
}