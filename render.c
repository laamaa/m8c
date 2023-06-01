// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include "render.h"

#include <SDL.h>
#include <stdio.h>

#include "SDL2_inprint.h"
#include "command.h"
#include "fx_cube.h"

#include "inline_font.h"
#include "inline_font_large.h"
#include "inline_font_small.h"

SDL_Window *win;
SDL_Renderer *rend;
SDL_Texture *maintexture;
SDL_Color background_color =
    (SDL_Color){.r = 0x00, .g = 0x00, .b = 0x00, .a = 0x00};

static uint32_t ticks_fps;
static int fps;
static int large_font_enabled = 0;
static int screen_offset_y = 0;

uint8_t fullscreen = 0;

static uint8_t dirty = 0;

// Initializes SDL and creates a renderer and required surfaces
int initialize_sdl(int init_fullscreen, int init_use_gpu) {
  // ticks = SDL_GetTicks();

  const int window_width = 640;  // SDL window width
  const int window_height = 480; // SDL window height

  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "SDL_Init: %s\n", SDL_GetError());
    return -1;
  }
  // SDL documentation recommends this
  atexit(SDL_Quit);

  win = SDL_CreateWindow("m8c", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         window_width, window_height,
                         SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL |
                             SDL_WINDOW_RESIZABLE | init_fullscreen);

  rend = SDL_CreateRenderer(
      win, -1, init_use_gpu ? SDL_RENDERER_ACCELERATED : SDL_RENDERER_SOFTWARE);

  SDL_RenderSetLogicalSize(rend, 320, 240);

  maintexture = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_TARGET, 320, 240);

  SDL_SetRenderTarget(rend, maintexture);

  SDL_SetRenderDrawColor(rend, background_color.r, background_color.g,
                         background_color.b, background_color.a);

  SDL_RenderClear(rend);

  // Initialize a texture for the font and read the inline font bitmap
  inrenderer(rend);
  struct inline_font *font = &inline_font_small;
  prepare_inline_font(font->bits, font->width, font->height);

  SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);

  dirty = 1;

  return 1;
}

static void change_font(struct inline_font *font) {
  kill_inline_font();
  prepare_inline_font(font->bits, font->width, font->height);
}

void set_large_mode(int enabled) {
  if (enabled) {
    large_font_enabled = 1;
    screen_offset_y = 40;
    change_font(&inline_font_large);
  } else {
    large_font_enabled = 0;
    screen_offset_y = 0;
    change_font(&inline_font_small);
  }
}

void close_renderer() {
  kill_inline_font();
  SDL_DestroyTexture(maintexture);
  SDL_DestroyRenderer(rend);
  SDL_DestroyWindow(win);
}

void toggle_fullscreen() {

  int fullscreen_state = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN;

  SDL_SetWindowFullscreen(win,
                          fullscreen_state ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
  SDL_ShowCursor(fullscreen_state);

  dirty = 1;
}

int draw_character(struct draw_character_command *command) {

  uint32_t fgcolor = (command->foreground.r << 16) |
                     (command->foreground.g << 8) | command->foreground.b;
  uint32_t bgcolor = (command->background.r << 16) |
                     (command->background.g << 8) | command->background.b;

  /* Notes:
     If large font is enabled, offset the screen elements by a fixed amount.
     If background and foreground colors are the same, draw transparent
     background. Due to the font bitmaps, a different pixel offset is needed for
     both*/

  inprint(rend, (char *)&command->c, command->pos.x,
          command->pos.y + (large_font_enabled ? 2 : 3) - screen_offset_y,
          fgcolor, (bgcolor == fgcolor) ? -1 : bgcolor);

  dirty = 1;

  return 1;
}

void draw_rectangle(struct draw_rectangle_command *command) {

  SDL_Rect render_rect;

  render_rect.x = command->pos.x;
  if (large_font_enabled == 1) {
    render_rect.y = command->pos.y - screen_offset_y;
  } else {
    render_rect.y = command->pos.y;
  }
  render_rect.h = command->size.height;
  render_rect.w = command->size.width;

  // Background color changed
  if (render_rect.x == 0 && render_rect.y == 0 && render_rect.w == 320 &&
      (render_rect.h == 240 || render_rect.h == 320)) {
    background_color.r = command->color.r;
    background_color.g = command->color.g;
    background_color.b = command->color.b;
    background_color.a = 0xFF;

#ifdef __ANDROID__
    int bgcolor =
        (command->color.r << 16) | (command->color.g << 8) | command->color.b;
    SDL_AndroidSendMessage(0x8001, bgcolor);
#endif
  }

  SDL_SetRenderDrawColor(rend, command->color.r, command->color.g,
                         command->color.b, 0xFF);
  SDL_RenderFillRect(rend, &render_rect);

  dirty = 1;
}

void draw_waveform(struct draw_oscilloscope_waveform_command *command) {

  static uint8_t wfm_cleared = 0;
  static int prev_waveform_size = 0;

  // If the waveform is not being displayed and it's already been cleared, skip
  // rendering it
  if (!(wfm_cleared && command->waveform_size == 0)) {

    SDL_Rect wf_rect;
    if (command->waveform_size > 0) {
      wf_rect.x = 320 - command->waveform_size;
      wf_rect.y = 0;
      wf_rect.w = command->waveform_size;
      wf_rect.h = 21;
    } else {
      wf_rect.x = 320 - prev_waveform_size;
      wf_rect.y = 0;
      wf_rect.w = prev_waveform_size;
      wf_rect.h = 21;
    }
    prev_waveform_size = command->waveform_size;

    SDL_SetRenderDrawColor(rend, background_color.r, background_color.g,
                           background_color.b, background_color.a);
    SDL_RenderFillRect(rend, &wf_rect);

    SDL_SetRenderDrawColor(rend, command->color.r, command->color.g,
                           command->color.b, 255);

    // Create a SDL_Point array of the waveform pixels for batch drawing
    SDL_Point waveform_points[command->waveform_size];

    for (int i = 0; i < command->waveform_size; i++) {
      // Limit value because the oscilloscope commands seem to glitch
      // occasionally
      if (command->waveform[i] > 20) {
        command->waveform[i] = 20;
      }
      waveform_points[i].x = i + wf_rect.x;
      waveform_points[i].y = command->waveform[i];
    }

    SDL_RenderDrawPoints(rend, waveform_points, command->waveform_size);

    // The packet we just drew was an empty waveform
    if (command->waveform_size == 0) {
      wfm_cleared = 1;
    } else {
      wfm_cleared = 0;
    }

    dirty = 1;
  }
}

void display_keyjazz_overlay(uint8_t show, uint8_t base_octave,
                             uint8_t velocity) {

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
    dcc.pos.x = 296;
    dcc.pos.y = 226;

    draw_character(&dcc);

    char buf[8];
    snprintf(buf, sizeof(buf), "%02X %u", velocity, base_octave);

    for (int i = 3; i >= 0; i--) {
      dcc.c = buf[i];
      draw_character(&dcc);
      dcc.pos.x -= 8;
    }

  } else {
    struct draw_rectangle_command drc;
    drc.color = (struct color){background_color.r, background_color.g,
                               background_color.b};
    drc.pos.x = 272;
    drc.pos.y = 226;
    drc.size.width = 45;
    drc.size.height = 14;

    draw_rectangle(&drc);
  }

  dirty = 1;
}

void render_screen() {
  if (dirty) {
    dirty = 0;
    // ticks = SDL_GetTicks();
    SDL_SetRenderTarget(rend, NULL);

    SDL_SetRenderDrawColor(rend, background_color.r, background_color.g,
                           background_color.b, background_color.a);

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

void screensaver_init() {
  set_large_mode(1);
  fx_cube_init(rend, (SDL_Color){255, 255, 255, 255});
  SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Screensaver initialized");
}

void screensaver_draw() {
  fx_cube_update();
  dirty = 1;
}

void screensaver_destroy() {
  fx_cube_destroy();
  SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Screensaver destroyed");
}
