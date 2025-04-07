// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef RENDER_H_
#define RENDER_H_

#include "command.h"
#include "config.h"

#include <stdint.h>

int renderer_initialize(config_params_s *conf);
void renderer_close();
void renderer_set_font_mode(int mode);

void draw_waveform(struct draw_oscilloscope_waveform_command *command);
void draw_rectangle(struct draw_rectangle_command *command);
int draw_character(struct draw_character_command *command);

void set_m8_model(unsigned int model);

void render_screen();
void toggle_fullscreen();
void display_keyjazz_overlay(uint8_t show, uint8_t base_octave, uint8_t velocity);

void show_error_message(const char *message);

void screensaver_init();
void screensaver_draw();
void screensaver_destroy();

void fix_texture_scaling_after_window_resize(void);

#endif
