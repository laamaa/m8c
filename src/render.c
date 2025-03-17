// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include "render.h"

#include <SDL3/SDL.h>
#include <stdio.h>

#include "SDL2_inprint.h"
#include "command.h"
#include "fx_cube.h"

#include "font1.h"
#include "font2.h"
#include "font3.h"
#include "font4.h"
#include "font5.h"
#include "inline_font.h"

#include <stdlib.h>

SDL_Window *win;
SDL_Renderer *rend;
SDL_Texture *main_texture;
SDL_Color global_background_color = (SDL_Color){.r = 0x00, .g = 0x00, .b = 0x00, .a = 0x00};
SDL_RendererLogicalPresentation scaling_mode = SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;

static uint32_t ticks_fps;
static int fps;
static int font_mode = -1;
static unsigned int m8_hardware_model = 0;
static int screen_offset_y = 0;
static int text_offset_y = 0;
static int waveform_max_height = 24;

static int texture_width = 320;
static int texture_height = 240;

struct inline_font *fonts[5] = {&font_v1_small, &font_v1_large, &font_v2_small, &font_v2_large,
                                &font_v2_huge};

uint8_t fullscreen = 0;

static uint8_t dirty = 0;

// Initializes SDL and creates a renderer and required surfaces
int initialize_sdl(const unsigned int init_fullscreen) {

  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD) == false) {
    SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "SDL_Init: %s\n", SDL_GetError());
    return false;
  }

  // SDL documentation recommends this
  atexit(SDL_Quit);

  win = SDL_CreateWindow("m8c", texture_width * 2, texture_height * 2,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | init_fullscreen);

  rend = SDL_CreateRenderer(win, NULL);

  SDL_SetRenderLogicalPresentation(rend, texture_width, texture_height, scaling_mode);

  main_texture = NULL;
  main_texture = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                                   texture_width, texture_height);

  SDL_SetTextureScaleMode(main_texture, SDL_SCALEMODE_NEAREST);

  SDL_SetRenderTarget(rend, main_texture);

  SDL_SetRenderDrawColor(rend, global_background_color.r, global_background_color.g,
                         global_background_color.b, global_background_color.a);

  SDL_RenderClear(rend);

  set_font_mode(0);

  SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);

  dirty = 1;

  return 1;
}

static void change_font(struct inline_font *font) {
  kill_inline_font();
  inrenderer(rend);
  prepare_inline_font(font);
}

static void check_and_adjust_window_and_texture_size(const int new_width, const int new_height) {

  if (texture_width == new_width && texture_height == new_height) {
    return;
  }

  int window_h, window_w;

  texture_width = new_width;
  texture_height = new_height;

  // Query window size and resize if smaller than default
  SDL_GetWindowSize(win, &window_w, &window_h);
  if (window_w < texture_width * 2 || window_h < texture_height * 2) {
    SDL_SetWindowSize(win, texture_width * 2, texture_height * 2);
  }

  SDL_DestroyTexture(main_texture);
  SDL_SetRenderLogicalPresentation(rend, texture_width, texture_height, scaling_mode);
  main_texture = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                                   texture_width, texture_height);
  SDL_SetTextureScaleMode(main_texture, SDL_SCALEMODE_NEAREST);
  SDL_SetRenderTarget(rend, main_texture);
}

// Set M8 hardware model in use. 0 = MK1, 1 = MK2
void set_m8_model(const unsigned int model) {

  if (model == 1) {
    m8_hardware_model = 1;
    check_and_adjust_window_and_texture_size(480, 320);
  } else {
    m8_hardware_model = 0;
    check_and_adjust_window_and_texture_size(320, 240);
  }
}

void set_font_mode(int mode) {
  if (mode < 0 || mode > 2) {
    // bad font mode
    return;
  }
  if (m8_hardware_model == 1) {
    mode += 2;
  }
  if (font_mode == mode)
    return;

  font_mode = mode;
  screen_offset_y = fonts[mode]->screen_offset_y;
  text_offset_y = fonts[mode]->text_offset_y;
  waveform_max_height = fonts[mode]->waveform_max_height;

  change_font(fonts[mode]);
  SDL_LogDebug(SDL_LOG_CATEGORY_RENDER, "Font mode %i, Screen offset %i", mode, screen_offset_y);
}

void close_renderer() {
  kill_inline_font();
  SDL_DestroyTexture(main_texture);
  SDL_DestroyRenderer(rend);
  SDL_DestroyWindow(win);
}

void toggle_fullscreen() {

  const unsigned long fullscreen_state = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN;

  SDL_SetWindowFullscreen(win, fullscreen_state ? false : true);
  SDL_SyncWindow(win);
  if (fullscreen_state) {
    // Show cursor when in windowed state
    SDL_ShowCursor();
  } else {
    SDL_HideCursor();
  }

  dirty = 1;
}

int draw_character(struct draw_character_command *command) {

  const uint32_t fgcolor =
      command->foreground.r << 16 | command->foreground.g << 8 | command->foreground.b;
  const uint32_t bgcolor =
      command->background.r << 16 | command->background.g << 8 | command->background.b;

  /* Notes:
     If large font is enabled, offset the screen elements by a fixed amount.
     If background and foreground colors are the same, draw transparent
     background. Due to the font bitmaps, a different pixel offset is needed for
     both*/

  inprint(rend, (char *)&command->c, command->pos.x,
          command->pos.y + text_offset_y + screen_offset_y, fgcolor, bgcolor);

  dirty = 1;

  return 1;
}

void draw_rectangle(struct draw_rectangle_command *command) {

  SDL_FRect render_rect;

  render_rect.x = (float)command->pos.x;
  render_rect.y = (float)(command->pos.y + screen_offset_y);
  render_rect.h = command->size.height;
  render_rect.w = command->size.width;

  // Background color changed
  if (render_rect.x == 0 && render_rect.y <= 0 && render_rect.w == (float)texture_width &&
      render_rect.h >= (float)texture_height) {
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "BG color change: %d %d %d", command->color.r,
                 command->color.g, command->color.b);
    global_background_color.r = command->color.r;
    global_background_color.g = command->color.g;
    global_background_color.b = command->color.b;
    global_background_color.a = 0xFF;
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "x:%f, y:%f, w:%f, h:%f", render_rect.x,
                 render_rect.y, render_rect.w, render_rect.h);

#ifdef __ANDROID__
    int bgcolor = (command->color.r << 16) | (command->color.g << 8) | command->color.b;
    SDL_AndroidSendMessage(0x8001, bgcolor);
#endif
  }

  SDL_SetRenderDrawColor(rend, command->color.r, command->color.g, command->color.b, 0xFF);
  SDL_RenderFillRect(rend, &render_rect);

  dirty = 1;
}

void draw_waveform(struct draw_oscilloscope_waveform_command *command) {

  static uint8_t wfm_cleared = 0;
  static int prev_waveform_size = 0;

  // If the waveform is not being displayed, and it's already been cleared, skip rendering it
  if (!(wfm_cleared && command->waveform_size == 0)) {

    SDL_FRect wf_rect;
    if (command->waveform_size > 0) {
      wf_rect.x = (float)(texture_width - command->waveform_size);
      wf_rect.y = 0;
      wf_rect.w = command->waveform_size;
      wf_rect.h = (float)(waveform_max_height + 1);
    } else {
      wf_rect.x = (float)(texture_width - prev_waveform_size);
      wf_rect.y = 0;
      wf_rect.w = (float)prev_waveform_size;
      wf_rect.h = (float)(waveform_max_height + 1);
    }
    prev_waveform_size = command->waveform_size;

    SDL_SetRenderDrawColor(rend, global_background_color.r, global_background_color.g,
                           global_background_color.b, global_background_color.a);
    SDL_RenderFillRect(rend, &wf_rect);

    SDL_SetRenderDrawColor(rend, command->color.r, command->color.g, command->color.b, 255);

    // Create a SDL_Point array of the waveform pixels for batch drawing
    SDL_FPoint waveform_points[command->waveform_size];

    for (int i = 0; i < command->waveform_size; i++) {
      // Limit value to avoid random glitches
      if (command->waveform[i] > waveform_max_height) {
        command->waveform[i] = waveform_max_height;
      }
      waveform_points[i].x = (float)i + wf_rect.x;
      waveform_points[i].y = command->waveform[i];
    }

    SDL_RenderPoints(rend, waveform_points, command->waveform_size);

    // The packet we just drew was an empty waveform
    if (command->waveform_size == 0) {
      wfm_cleared = 1;
    } else {
      wfm_cleared = 0;
    }

    dirty = 1;
  }
}

void display_keyjazz_overlay(const uint8_t show, const uint8_t base_octave,
                             const uint8_t velocity) {

  const Uint16 overlay_offset_x = texture_width - (fonts[font_mode]->glyph_x * 7 + 1);
  const Uint16 overlay_offset_y = texture_height - (fonts[font_mode]->glyph_y + 1);
  const Uint32 bg_color =
      global_background_color.r << 16 | global_background_color.g << 8 | global_background_color.b;

  if (show) {
    char overlay_text[7];
    snprintf(overlay_text, sizeof(overlay_text), "%02X %u", velocity, base_octave);
    inprint(rend, overlay_text, overlay_offset_x, overlay_offset_y, 0xC8C8C8, bg_color);
    inprint(rend, "*", overlay_offset_x + (fonts[font_mode]->glyph_x * 5 + 5), overlay_offset_y,
            0xFF0000, bg_color);
  } else {
    inprint(rend, "      ", overlay_offset_x, overlay_offset_y, 0xC8C8C8, bg_color);
  }

  dirty = 1;
}

void render_screen() {
  if (dirty) {
    dirty = 0;
    SDL_SetRenderTarget(rend, NULL);

    SDL_SetRenderDrawColor(rend, global_background_color.r, global_background_color.g,
                           global_background_color.b, global_background_color.a);

    SDL_RenderClear(rend);
    SDL_RenderTexture(rend, main_texture, NULL, NULL);
    SDL_RenderPresent(rend);
    SDL_SetRenderTarget(rend, main_texture);

    fps++;

    if (SDL_GetTicks() - ticks_fps > 5000) {
      ticks_fps = SDL_GetTicks();
      SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "%.1f fps\n", (float)fps / 5);
      fps = 0;
    }
  }
}

void screensaver_init() {
  set_font_mode(1);
  fx_cube_init(rend, (SDL_Color){255, 255, 255, 255}, texture_width, texture_height,
               fonts[font_mode]->glyph_x);
  SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Screensaver initialized");
}

void screensaver_draw() {
  fx_cube_update();
  dirty = 1;
}

void screensaver_destroy() {
  fx_cube_destroy();
  set_font_mode(0);
  SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Screensaver destroyed");
}
