// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef CONFIG_H_
#define CONFIG_H_

#include "ini.h"

typedef struct config_params_s {
  char *filename;
  int init_fullscreen;
  int init_use_gpu;
  int idle_ms;
  int wait_for_device;

  int key_up;
  int key_left;
  int key_down;
  int key_right;
  int key_select;
  int key_select_alt;
  int key_start;
  int key_start_alt;
  int key_opt;
  int key_opt_alt;
  int key_edit;
  int key_edit_alt;
  int key_delete;
  int key_reset;

  int gamepad_up;
  int gamepad_left;
  int gamepad_down;
  int gamepad_right;
  int gamepad_select;
  int gamepad_start;
  int gamepad_opt;
  int gamepad_edit;
  int gamepad_quit;
  int gamepad_reset;

  int gamepad_analog_threshold;
  int gamepad_analog_invert;
  int gamepad_analog_axis_updown;
  int gamepad_analog_axis_leftright;
  int gamepad_analog_axis_start;
  int gamepad_analog_axis_select;
  int gamepad_analog_axis_opt;
  int gamepad_analog_axis_edit;

} config_params_s;


config_params_s init_config();
void read_config();
void read_graphics_config(ini_t *config, config_params_s *conf);
void read_key_config(ini_t *config, config_params_s *conf);
void read_gamepad_config(ini_t *config, config_params_s *conf);

#endif