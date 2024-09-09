//
// Created by jonne on 9/8/24.
//

#include "fx_piano.h"
#include "render.h"
#include "inline_font.h"
#include <SDL2/SDL.h>

struct active_notes {
  uint8_t note;
  uint8_t sharp;
  uint8_t octave;
};

static int width = 320;
static int height = 240;
static SDL_Texture *fx_texture;
static uint32_t *framebuffer;
struct active_notes active_notes[8] = {0};
static int is_initialized = 0;

static const uint32_t bgcolor = 0xA0000000;

// Pastel Rainbow from https://colorkit.co/palettes/8-colors/
uint32_t palette[8] = {0xEFFFADAD, 0xEFFFD6A5, 0xEFFDFFB6, 0xEFCAFFBF,
                       0xEF9BF6FF, 0xEFA0C4FF, 0xEFBDB2FF, 0xEFFFC6FF};

static int get_active_note_from_channel(int ch) {
  if (ch >= 0 && ch < 8)
    return active_notes[ch].note + active_notes[ch].sharp + active_notes[ch].octave;
  else
    return 0;
}

void fx_piano_init(SDL_Texture *output_texture) {
  fx_texture = output_texture;
  SDL_QueryTexture(fx_texture, NULL, NULL, &width, &height);
  framebuffer = SDL_malloc(sizeof(uint32_t) * width * height);
  SDL_memset4(framebuffer, bgcolor, width * height);
  is_initialized = 1;
}

void fx_piano_update() {
  //Shift everything up by one row and clear bottom row
  for (int y = 1; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int buffer_pos = x + (y * width);
      // Top rows have a fading alpha value
      if (y<60 && (framebuffer[buffer_pos] >> 24) > 0x04){
        framebuffer[buffer_pos-width] = framebuffer[buffer_pos]-((0x05 << 24));
      } else {
        framebuffer[buffer_pos-width] = framebuffer[buffer_pos];
      }
    }
  }

  for (int x = 0; x < width; x++) {
    framebuffer[width*(height-1)+x] = bgcolor;
    framebuffer[x] = bgcolor;
  }

  int notes[8];
  for (int i = 0; i < 8; i++) {
    notes[i] = get_active_note_from_channel(i);

    int last_row = (height-1)*width;
    int spacing = width / 128;
    if (notes[i]>0) {
      int pos = last_row + (notes[i] * spacing);
      uint32_t color = (framebuffer[pos] + palette[i]) | (0xCF <<24);
      for (int bar_width = 0; bar_width < spacing; bar_width++) {
        framebuffer[pos++] = color;
      }
      framebuffer[pos] = color | (0x20 << 24);
    }
  }

  SDL_UpdateTexture(fx_texture, NULL, framebuffer,
                    width * sizeof(framebuffer[0]));
};

void fx_piano_destroy() {
  if (is_initialized) {
    SDL_free(framebuffer);
    is_initialized = 0;
  }
}

/* Converts the note characters displayed on M8 screen to hex values for use in
 * visualizers */
int note_to_hex(int note_char) {
  switch (note_char) {
  case 45: // - = note off
    return 0x00;
  case 67: // C
    return 0x01;
  case 68: // D
    return 0x03;
  case 69: // E
    return 0x05;
  case 70: // F
    return 0x06;
  case 71: // G
    return 0x08;
  case 65: // A
    return 0x0A;
  case 66: // B
    return 0x0C;
  default:
    return 0x00;
  }
}

// Updates the active notes information array with the current command's data
void update_active_notes_data(struct draw_character_command *command, int font_mode) {

  int line_spacing;
  int character_spacing;
  int playback_info_offset_x;

  switch (font_mode) {
    case 1:
    case 4:
      // Note name sidebar not visible all the time on MK1 / MK2
      return;
    case 2:
    case 3:
      line_spacing = 14;
      character_spacing = 12;
      playback_info_offset_x = 432;
      break;
    default:
      line_spacing = 10;
      character_spacing = 8;
      playback_info_offset_x = 288;
      break;
  }

  // Channels are 10 pixels apart, starting from y=70
  int channel_number = command->pos.y / line_spacing - 7;


  // Note playback information starts from X=288
  if (command->pos.x == playback_info_offset_x) {
    active_notes[channel_number].note = note_to_hex(command->c);
  }

  // Sharp notes live at x = 296
  if (command->pos.x == playback_info_offset_x + character_spacing && !strcmp((char *)&command->c, "#"))
    active_notes[channel_number].sharp = 1;
  else
    active_notes[channel_number].sharp = 0;

  // Note octave, x = 304
  if (command->pos.x == playback_info_offset_x + 2*character_spacing) {
    int8_t octave = command->c - 48;

    // Octaves A-B
    if (octave == 17 || octave == 18)
      octave -= 7;

    // If octave hasn't been applied to the note yet, do it
    if (octave > 0)
      active_notes[channel_number].octave = octave * 0x0C;
    else
      active_notes[channel_number].octave = 0;
  }
}