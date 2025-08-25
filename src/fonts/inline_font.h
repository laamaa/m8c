#ifndef INLINE_FONT_H_
#define INLINE_FONT_H_

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

#endif
