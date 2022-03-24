// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include "render.h"

#include <SDL.h>
#include <stdio.h>

#include "SDL2_inprint.h"
#include "command.h"

SDL_Window *win;
SDL_Renderer *rend;
SDL_Texture *maintexture;
SDL_Color background_color = (SDL_Color){0, 0, 0, 0};

static uint32_t ticks;
static uint32_t ticks_fps;
static int fps;
uint8_t fullscreen = 0;

// Initializes SDL and creates a renderer and required surfaces
int initialize_sdl(int init_fullscreen, int init_use_gpu) {

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
                             SDL_WINDOW_RESIZABLE | init_fullscreen);

  rend = SDL_CreateRenderer(win, -1, init_use_gpu ? SDL_RENDERER_ACCELERATED : SDL_RENDERER_SOFTWARE);

  SDL_RenderSetLogicalSize(rend, 320, 240);

  maintexture = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_TARGET, 320, 240);

  SDL_SetRenderTarget(rend, maintexture);

  SDL_SetRenderDrawColor(rend, 0x00, 0x00, 0x00, 0x00);
  SDL_RenderClear(rend);

  // Initialize a texture for the font and read the inline font bitmap
  inrenderer(rend);
  prepare_inline_font();

  SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);

  // Uncomment this for debug level logging
  // SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);

  return 1;
}

void close_renderer() {
  SDL_DestroyTexture(maintexture);
  SDL_DestroyRenderer(rend);
  SDL_DestroyWindow(win);
}

void toggle_fullscreen() {

  int fullscreen_state = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN;

  SDL_SetWindowFullscreen(win, fullscreen_state ? 0 : SDL_WINDOW_FULLSCREEN);
  SDL_ShowCursor(fullscreen_state);
}

int draw_character(struct draw_character_command *command) {

  uint32_t fgcolor = (command->foreground.r << 16) |
                     (command->foreground.g << 8) | command->foreground.b;
  uint32_t bgcolor = (command->background.r << 16) |
                     (command->background.g << 8) | command->background.b;

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
    background_color.a = 0xFF;
  }

  SDL_SetRenderDrawColor(rend, command->color.r, command->color.g,
                         command->color.b, 0xFF);
  SDL_RenderFillRect(rend, &render_rect);
}

void draw_waveform(struct draw_oscilloscope_waveform_command *command) {

  const SDL_Rect wf_rect = {0, 0, 320, 21};

  SDL_SetRenderDrawColor(rend, background_color.r, background_color.g,
                         background_color.b, background_color.a);
  SDL_RenderFillRect(rend, &wf_rect);

  SDL_SetRenderDrawColor(rend, command->color.r, command->color.g,
                         command->color.b, 255);

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

  if (SDL_GetTicks() - ticks > 14) {
    ticks = SDL_GetTicks();
    SDL_SetRenderTarget(rend, NULL);
    SDL_SetRenderDrawColor(rend, 0, 0, 0, 0);
    SDL_RenderClear(rend);
    SDL_RenderCopy(rend, maintexture, NULL, NULL);
    SDL_RenderPresent(rend);
    SDL_SetRenderTarget(rend, maintexture);

    fps++;

    if (SDL_GetTicks() - ticks_fps > 5000) {
      ticks_fps = SDL_GetTicks();
      SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "%.1f fps\n", (float)fps / 5);
      fps = 0;
    }
  }
}
