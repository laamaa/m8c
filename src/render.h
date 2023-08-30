// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef RENDER_H_
#define RENDER_H_

#include "command.h"

int initialize_sdl(int init_fullscreen, int init_use_gpu);
void close_renderer();

void draw_waveform(struct draw_oscilloscope_waveform_command *command);
void draw_rectangle(struct draw_rectangle_command *command);
int draw_character(struct draw_character_command *command);
void set_large_mode(int enabled);
void view_changed(int view);

void render_screen();
void toggle_fullscreen();
void display_keyjazz_overlay(uint8_t show, uint8_t base_octave, uint8_t velocity);

void screensaver_init();
void screensaver_draw();
void screensaver_destroy();

#endif
