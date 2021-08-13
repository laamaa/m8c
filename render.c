// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include "render.h"

#include <SDL2/SDL_log.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include <stdio.h>

#include "SDL2_inprint.h"
#include "SDL_blendmode.h"
#include "command.h"
#include "fx_cube.h"
#include "fx_piano.h"
#include "fx_tunnel.h"

SDL_Window *win;
SDL_Renderer *rend;
SDL_Texture *maintexture;
SDL_Texture *fxtexture;

// Device state information
SDL_Color background_color = (SDL_Color){0, 0, 0, 0};
SDL_Color foreground_color = (SDL_Color){0, 0, 0, 0};
struct active_notes active_notes[8] = {0};
int vu_meter[2] = {0};

static uint32_t ticks;
#ifdef SHOW_FPS
static uint32_t ticks_fps;
static int fps;
#endif
uint8_t fullscreen = 0;
uint8_t special_fx = 0;

// Initializes SDL and creates a renderer and required surfaces
int initialize_sdl() {

  ticks = SDL_GetTicks();

  const int window_width = 640;  // SDL window width
  const int window_height = 480; // SDL window height

  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "SDL_Init: %s\n", SDL_GetError());
    return -1;
  }

  win = SDL_CreateWindow("m8c", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         window_width, window_height,
                         SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL |
                             SDL_WINDOW_RESIZABLE);

  rend = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

  SDL_RenderSetLogicalSize(rend, 320, 240);

  maintexture = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_TARGET, 320, 240);

  fxtexture = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, 320, 240);

  SDL_SetTextureBlendMode(maintexture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureBlendMode(fxtexture, SDL_BLENDMODE_BLEND);

  SDL_SetRenderTarget(rend, fxtexture);
  SDL_SetRenderDrawColor(rend, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(rend);

  SDL_SetRenderTarget(rend, maintexture);
  SDL_SetRenderDrawColor(rend, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(rend);

  // Initialize a texture for the font and read the inline font bitmap
  inrenderer(rend);
  prepare_inline_font();

  return 1;
}

void close_renderer() {
  // Destroy special fx if running
  switch (special_fx) {
  case 1:
    fx_piano_destroy();
    break;
  case 2:
    fx_tunnel_destroy();
    break;
  case 3:
    fx_cube_destroy();
  default:
    break;
  }
  special_fx = 0;

  // Destroy sdl stuff
  SDL_DestroyTexture(maintexture);
  SDL_DestroyTexture(fxtexture);
  SDL_DestroyRenderer(rend);
  SDL_DestroyWindow(win);
}

void toggle_fullscreen() {

  int fullscreen_state = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN;

  SDL_SetWindowFullscreen(win, fullscreen_state ? 0 : SDL_WINDOW_FULLSCREEN);
  SDL_ShowCursor(fullscreen_state);
}

int get_active_note_from_channel(int ch) {
  if (ch >= 0 && ch < 8)
    return active_notes[ch].note + active_notes[ch].sharp +
           active_notes[ch].octave;
  else
    return 0;
}

int toggle_special_fx() {
  special_fx++;
  switch (special_fx) {
  case 1:
    fx_piano_init(fxtexture);
    break;
  case 2:
    fx_piano_destroy();
    fx_tunnel_init(fxtexture);
    break;
  case 3:
    fx_tunnel_destroy();
    fx_cube_init(rend, foreground_color);
    break;
  case 4:
    fx_cube_destroy();
    special_fx = 0;
  default:
    break;
  }
  return special_fx;
}

void render_special_fx() {
  switch (special_fx) {
  case 1:
    fx_piano_update();
    break;
  case 2:
    fx_tunnel_update();
    break;
  case 3:
    fx_cube_update();
  default:
    break;
  }
}

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
void update_active_notes_data(struct draw_character_command *command) {

  // Channels are 10 pixels apart, starting from y=70
  int channel_number = command->pos.y / 10 - 7;

  // Note playback information starts from X=288
  if (command->pos.x == 288) {
    active_notes[channel_number].note = note_to_hex(command->c);
  }

  // Sharp notes live at x = 296
  if (command->pos.x == 296 && !strcmp((char *)&command->c, "#"))
    active_notes[channel_number].sharp = 1;
  else
    active_notes[channel_number].sharp = 0;

  // Note octave, x = 304
  if (command->pos.x == 304) {
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

int draw_character(struct draw_character_command *command) {

  uint32_t fgcolor = (command->foreground.r << 16) |
                     (command->foreground.g << 8) | command->foreground.b;
  uint32_t bgcolor = (command->background.r << 16) |
                     (command->background.g << 8) | command->background.b;

  //SDL_Log("%s: %d %d", (char *)&command->c, command->pos.x, command->pos.y);

  /* Note characters appear between y=70 and y=140, update the active notes
    array */
  if (command->pos.y >= 70 && command->pos.y <= 140) {
    update_active_notes_data(command);
  }

  /* Get the foreground color from tempo text, that should be pretty much always visible */
  if (command->pos.x == 272 && command->pos.y == 50) {
    foreground_color = (SDL_Color){command->foreground.r, command->foreground.g,
                                   command->foreground.b, SDL_ALPHA_OPAQUE};
  }

  if (bgcolor == fgcolor) {
    // When bgcolor and fgcolor are the same, do not render a background
    inprint(rend, (char *)&command->c, command->pos.x, command->pos.y + 3,
            fgcolor, -1);
  } else {
    inprint(rend, (char *)&command->c, command->pos.x, command->pos.y + 3,
            fgcolor, bgcolor);
  }

  return 1;
}

void update_vu_meter_data(struct draw_rectangle_command *command) {
  int level = command->pos.x - 271;
  if (command->pos.y == 30) {
    vu_meter[0] = level;
  } else {
    vu_meter[1] = level;
  }
}

int get_vu_meter_data(int ch) {
  if (ch < 2)
    return vu_meter[ch];
  else
    return 0;
}

void draw_rectangle(struct draw_rectangle_command *command) {

  SDL_Rect render_rect;

  render_rect.x = command->pos.x;
  render_rect.y = command->pos.y;
  render_rect.h = command->size.height;
  render_rect.w = command->size.width;

  // Background color changed
  if (render_rect.x == 0 && render_rect.y == 0 && render_rect.w == 320 &&
      render_rect.h == 240) {
    background_color.r = command->color.r;
    background_color.g = command->color.g;
    background_color.b = command->color.b;
    background_color.a = SDL_ALPHA_OPAQUE;
  }

  if ((render_rect.y == 30 || render_rect.y == 35) && render_rect.h == 4) {
    update_vu_meter_data(command);
  }

  // SDL_Log("x: %d y: %d h: %d w: %d", render_rect.x, render_rect.y,
  //         render_rect.h, render_rect.w);

  SDL_SetRenderDrawColor(rend, command->color.r, command->color.g,
                         command->color.b, SDL_ALPHA_OPAQUE);
  SDL_RenderFillRect(rend, &render_rect);
}

void draw_waveform(struct draw_oscilloscope_waveform_command *command) {

  const SDL_Rect wf_rect = {0, 0, 320, 21};

  SDL_SetRenderDrawColor(rend, background_color.r, background_color.g,
                         background_color.b, background_color.a);
  SDL_RenderFillRect(rend, &wf_rect);

  SDL_SetRenderDrawColor(rend, command->color.r, command->color.g,
                         command->color.b, SDL_ALPHA_OPAQUE);

  // Create a SDL_Point array of the waveform pixels for batch drawing
  SDL_Point waveform_points[command->waveform_size];

  for (int i = 0; i < command->waveform_size; i++) {
    // Limit value because the oscilloscope commands seem to glitch occasionally
    if (command->waveform[i] > 20)
      command->waveform[i] = 20;
    waveform_points[i].x = i;
    waveform_points[i].y = command->waveform[i];
  }

  SDL_RenderDrawPoints(rend, waveform_points, command->waveform_size);
}

void display_keyjazz_overlay(uint8_t show, uint8_t base_octave) {

  if (show) {
    struct draw_rectangle_command drc;
    drc.color = (struct color){255, 0, 0};
    drc.pos.x = 310;
    drc.pos.y = 230;
    drc.size.width = 5;
    drc.size.height = 5;

    draw_rectangle(&drc);

    struct draw_character_command dcc;
    dcc.background = (struct color){background_color.r, background_color.g,
                                    background_color.b};
    dcc.foreground = (struct color){200, 200, 200};
    dcc.c = base_octave + 48;
    dcc.pos.x = 300;
    dcc.pos.y = 226;

    draw_character(&dcc);

  } else {
    struct draw_rectangle_command drc;
    drc.color = (struct color){background_color.r, background_color.g,
                               background_color.b};
    drc.pos.x = 300;
    drc.pos.y = 226;
    drc.size.width = 20;
    drc.size.height = 14;

    draw_rectangle(&drc);
  }
}

void render_screen() {

  // process every 16ms (roughly 60fps)
  if (SDL_GetTicks() - ticks > 15) {
    ticks = SDL_GetTicks();
    SDL_SetRenderTarget(rend, NULL);
    SDL_SetRenderDrawColor(rend, 0, 0, 0, 0);
    SDL_RenderClear(rend);
    SDL_RenderCopy(rend, maintexture, NULL, NULL);
    if (special_fx) {
      render_special_fx();
      SDL_RenderCopy(rend, fxtexture, NULL, NULL);
    }

    SDL_RenderPresent(rend);
    SDL_SetRenderTarget(rend, maintexture);

#ifdef SHOW_FPS
    fps++;

    if (SDL_GetTicks() - ticks_fps > 5000) {
      ticks_fps = SDL_GetTicks();
      SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "%d fps\n", fps / 5);
      fps = 0;
    }
#endif
  }
}