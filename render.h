// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef RENDER_H_
#define RENDER_H_

#include "command.h"

struct active_notes {
    uint8_t note;
    uint8_t sharp;
    uint8_t octave;
};

int initialize_sdl();
void close_renderer();

int process_queues(struct command_queues *queues);
void draw_waveform(struct draw_oscilloscope_waveform_command *command);
void draw_rectangle(struct draw_rectangle_command *command);
int draw_character(struct draw_character_command *command);

void render_screen();
void toggle_fullscreen();
int toggle_special_fx();
void display_keyjazz_overlay(uint8_t show, uint8_t base_octave);

int get_active_note_from_channel(int ch);

int get_vu_meter_data(int ch);
 
#endif