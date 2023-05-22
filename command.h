// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef COMMAND_H_
#define COMMAND_H_

#include <stdint.h>

struct position {
  uint16_t x;
  uint16_t y;
};

struct size {
  uint16_t width;
  uint16_t height;
};

struct color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct draw_rectangle_command {
  struct position pos;
  struct size size;
  struct color color;
};

struct draw_character_command {
  int c;
  struct position pos;
  struct color foreground;
  struct color background;
};

struct draw_oscilloscope_waveform_command {
  struct color color;
  uint8_t waveform[320];
  uint16_t waveform_size;
};

int process_command(uint8_t *data, uint32_t size);

#endif
