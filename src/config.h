// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef CONFIG_H_
#define CONFIG_H_

#include "ini.h"

typedef struct config_params_s {
  char *filename;
  unsigned int init_fullscreen;
  unsigned int idle_ms;
  unsigned int wait_for_device;
  unsigned int wait_packets;
  unsigned int audio_enabled;
  unsigned int audio_buffer_size;
  char *audio_device_name;

  unsigned int key_up;
  unsigned int key_left;
  unsigned int key_down;
  unsigned int key_right;
  unsigned int key_select;
  unsigned int key_select_alt;
  unsigned int key_start;
  unsigned int key_start_alt;
  unsigned int key_opt;
  unsigned int key_opt_alt;
  unsigned int key_edit;
  unsigned int key_edit_alt;
  unsigned int key_delete;
  unsigned int key_reset;
  unsigned int key_jazz_inc_octave;
  unsigned int key_jazz_dec_octave;
  unsigned int key_jazz_inc_velocity;
  unsigned int key_jazz_dec_velocity;
  unsigned int key_toggle_audio;

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
void read_config(config_params_s *conf);
void read_audio_config(const ini_t *ini, config_params_s *conf);
void read_graphics_config(const ini_t *ini, config_params_s *conf);
void read_key_config(const ini_t *ini, config_params_s *conf);
void read_gamepad_config(const ini_t *ini, config_params_s *conf);

#endif
