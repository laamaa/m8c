// fonts.h
#ifndef FONTS_H
#define FONTS_H

#include <stddef.h>

struct inline_font {
  const int width;
  const int height;
  const int glyph_x;
  const int glyph_y;
  const int screen_offset_x;
  const int screen_offset_y;
  const int text_offset_y;
  const int waveform_max_height;
  const long image_size;
  const unsigned char image_data[];
};

// Number of available fonts
size_t fonts_count(void);

// Get a font by index (returns NULL if index is out of range)
const struct inline_font *fonts_get(size_t index);

// Get the whole font table (read-only). If count != NULL, it receives the length.
const struct inline_font *const *fonts_all(size_t *count);

#endif // FONTS_H