// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include "render.h"

#include <SDL3/SDL.h>

#include "SDL2_inprint.h"
#include "command.h"
#include "config.h"
#include "fx_cube.h"

#include "fonts/font1.h"
#include "fonts/font2.h"
#include "fonts/font3.h"
#include "fonts/font4.h"
#include "fonts/font5.h"
#include "fonts/inline_font.h"

#include <stdlib.h>

static SDL_Window *win;
static SDL_Renderer *rend;
static SDL_Texture *main_texture;
static SDL_Texture *hd_texture = NULL;
static SDL_Color global_background_color = (SDL_Color){.r = 0x00, .g = 0x00, .b = 0x00, .a = 0x00};
static SDL_RendererLogicalPresentation window_scaling_mode = SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
static SDL_ScaleMode texture_scaling_mode = SDL_SCALEMODE_NEAREST;

static uint32_t ticks_fps;
static int fps;
static int font_mode = -1;
static unsigned int m8_hardware_model = 1;
static int screen_offset_y = 0;
static int text_offset_y = 0;
static int waveform_max_height = 24;

static int texture_width = 480;
static int texture_height = 320;
static int hd_texture_width, hd_texture_height = 0;

static int screensaver_initialized = 0;

struct inline_font *fonts[5] = {&font_v1_small, &font_v1_large, &font_v2_small, &font_v2_large,
                                &font_v2_huge};

uint8_t fullscreen = 0;

static uint8_t dirty = 0;

void setup_hd_texture_scaling(void) {
  // Fullscreen scaling: use an intermediate texture with the highest possible integer size factor

  int window_width, window_height;
  if (!SDL_GetWindowSizeInPixels(win, &window_width, &window_height)) {
    SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Couldn't get window size: %s", SDL_GetError());
  }

  // Determine the texture aspect ratio
  const float texture_aspect_ratio = (float)texture_width / (float)texture_height;

  // Determine the window aspect ratio
  const float window_aspect_ratio = (float)window_width / (float)window_height;

  SDL_Texture *og_texture = SDL_GetRenderTarget(rend);
  SDL_SetRenderTarget(rend, NULL);
  // SDL forces black borders in letterbox mode, so in HD mode the texture scaling is manual
  SDL_SetRenderLogicalPresentation(rend, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
  SDL_SetTextureScaleMode(main_texture, SDL_SCALEMODE_NEAREST);

  // Check the aspect ratio to avoid unnecessary antialiasing
  if (texture_aspect_ratio == window_aspect_ratio) {
    SDL_SetTextureScaleMode(hd_texture, SDL_SCALEMODE_NEAREST);
  } else {
    SDL_SetTextureScaleMode(hd_texture, SDL_SCALEMODE_LINEAR);
  }
  SDL_SetRenderTarget(rend, og_texture);
}

// Creates an intermediate texture dynamically based on window size
static void create_hd_texture() {
  int window_width, window_height;

  // Get the current window size
  SDL_GetWindowSizeInPixels(win, &window_width, &window_height);

  // Calculate the maximum integer scaling factor
  int scale_factor = SDL_min(window_width / texture_width, window_height / texture_height);
  if (scale_factor < 1) {
    scale_factor = 1; // Ensure at least 1x scaling
  }

  // Calculate the HD texture size
  const int new_hd_texture_width = texture_width * scale_factor;
  const int new_hd_texture_height = texture_height * scale_factor;
  if (hd_texture != NULL && new_hd_texture_width == hd_texture_width && new_hd_texture_height == hd_texture_height) {
    // Texture exists and there is no change in the size, carry on
    SDL_LogDebug(SDL_LOG_CATEGORY_RENDER, "HD texture size not changed, skipping");
    return;
  }

  hd_texture_width = new_hd_texture_width;
  hd_texture_height = new_hd_texture_height;

  SDL_LogDebug(SDL_LOG_CATEGORY_RENDER, "Creating HD texture, scale factor: %d, size: %dx%d", scale_factor,
               hd_texture_width, hd_texture_height);

  // Destroy any existing HD texture
  if (hd_texture != NULL) {
    SDL_DestroyTexture(hd_texture);
  }

  // Create a new HD texture with the calculated size
  hd_texture = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                                 hd_texture_width, hd_texture_height);
  if (!hd_texture) {
    SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Couldn't create HD texture: %s", SDL_GetError());
  }

  // Optionally, set a linear scaling mode for smoother rendering
  if (!SDL_SetTextureScaleMode(hd_texture, SDL_SCALEMODE_LINEAR)) {
    SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Couldn't set HD texture scale mode: %s",
                    SDL_GetError());
  }

  setup_hd_texture_scaling();

}

static void change_font(struct inline_font *font) {
  inline_font_close();
  inline_font_set_renderer(rend);
  inline_font_initialize(font);
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

  if (hd_texture != NULL) {
    SDL_DestroyTexture(hd_texture);
    create_hd_texture(); // Create the texture dynamically based on window size
    setup_hd_texture_scaling();
  }

  if (main_texture != NULL) {
    SDL_DestroyTexture(main_texture);
  }

  main_texture = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                                   texture_width, texture_height);
  SDL_SetTextureScaleMode(main_texture, texture_scaling_mode);
  SDL_SetRenderTarget(rend, main_texture);
}

// Set the M8 hardware model in use. 0 = MK1, 1 = MK2
void set_m8_model(const unsigned int model) {

  if (model == 1) {
    m8_hardware_model = 1;
    check_and_adjust_window_and_texture_size(480, 320);
  } else {
    m8_hardware_model = 0;
    check_and_adjust_window_and_texture_size(320, 240);
  }
}

void renderer_set_font_mode(int mode) {
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

void renderer_close(void) {
  SDL_LogDebug(SDL_LOG_CATEGORY_RENDER, "Closing renderer");
  inline_font_close();
  if (main_texture != NULL) {
    SDL_DestroyTexture(main_texture);
  }
  if (hd_texture != NULL) {
    SDL_DestroyTexture(hd_texture);
  }
  SDL_DestroyRenderer(rend);
  SDL_DestroyWindow(win);
}

void toggle_fullscreen(void) {

  const unsigned long fullscreen_state = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN;

  SDL_SetWindowFullscreen(win, fullscreen_state ? false : true);
  SDL_SyncWindow(win);
  if (fullscreen_state) {
    // Show cursor when in a windowed state
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
     If a large font is enabled, offset the screen elements by a fixed amount.
     If background and foreground colors are the same, draw a transparent
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

    // Create an SDL_Point array of the waveform pixels for batch drawing
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
    SDL_snprintf(overlay_text, sizeof(overlay_text), "%02X %u", velocity, base_octave);
    inprint(rend, overlay_text, overlay_offset_x, overlay_offset_y, 0xC8C8C8, bg_color);
    inprint(rend, "*", overlay_offset_x + (fonts[font_mode]->glyph_x * 5 + 5), overlay_offset_y,
            0xFF0000, bg_color);
  } else {
    inprint(rend, "      ", overlay_offset_x, overlay_offset_y, 0xC8C8C8, bg_color);
  }

  dirty = 1;
}

static void log_fps_stats(void) {
  fps++;

  if (SDL_GetTicks() - ticks_fps > 5000) {
    ticks_fps = SDL_GetTicks();
    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "%.1f fps\n", (float)fps / 5);
    fps = 0;
  }
}

// Initializes SDL and creates a renderer and required surfaces
int renderer_initialize(config_params_s *conf) {

  // SDL documentation recommends this
  atexit(SDL_Quit);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == false) {
    SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "SDL_Init: %s", SDL_GetError());
    return 0;
  }

  if (!SDL_CreateWindowAndRenderer("m8c", texture_width * 2, texture_height * 2,
                                   SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                       SDL_WINDOW_OPENGL | conf->init_fullscreen,
                                   &win, &rend)) {
    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window and renderer: %s",
                    SDL_GetError());
    return false;
  }

  SDL_SetRenderVSync(rend, 1);

  if (!SDL_SetRenderLogicalPresentation(rend, texture_width, texture_height, window_scaling_mode)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set renderer logical presentation: %s",
                 SDL_GetError());
    return false;
  }

  main_texture = NULL;
  main_texture = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                                   texture_width, texture_height);

  if (main_texture == NULL) {
    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture: %s", SDL_GetError());
    return false;
  }

  if (conf->integer_scaling == 0) {
    // Create the HD texture dynamically based on window size
    create_hd_texture();
  }

  SDL_SetTextureScaleMode(main_texture, texture_scaling_mode);

  SDL_SetRenderTarget(rend, main_texture);

  SDL_SetRenderDrawColor(rend, global_background_color.r, global_background_color.g,
                         global_background_color.b, global_background_color.a);

  if (!SDL_RenderClear(rend)) {
    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Couldn't clear renderer: %s", SDL_GetError());
    return 0;
  }

  renderer_set_font_mode(0);

  SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "1");
  renderer_fix_texture_scaling_after_window_resize(conf); // iOS needs this, doesn't hurt on others either

  dirty = 1;

  SDL_PumpEvents();
  render_screen(conf);

  return 1;
}

void render_screen(config_params_s *conf) {
  if (!dirty) {
    // No draw commands have been issued since the last function call, do nothing
    return;
  }

  if (!conf) {
    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "render_screen configuration parameter is NULL.");
    return;
  }

  dirty = 0;

  if (!SDL_SetRenderTarget(rend, NULL)) {
    SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Couldn't set renderer target to window: %s",
                    SDL_GetError());
  }

  if (!SDL_SetRenderDrawColor(rend, global_background_color.r, global_background_color.g,
                              global_background_color.b, global_background_color.a)) {
    SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Couldn't set render draw color: %s", SDL_GetError());
  }

  if (!SDL_RenderClear(rend)) {
    SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Couldn't clear renderer: %s", SDL_GetError());
  }

  if (conf->integer_scaling) {
    // Direct rendering with integer scaling
    if (!SDL_RenderTexture(rend, main_texture, NULL, NULL)) {
      SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Couldn't render texture: %s", SDL_GetError());
    }
  } else {
    int window_width, window_height;
    if (!SDL_GetWindowSizeInPixels(win, &window_width, &window_height)) {
      SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Couldn't get window size: %s", SDL_GetError());
    }

    // Determine the texture aspect ratio
    const float texture_aspect_ratio = (float)texture_width / (float)texture_height;

    // Determine the window aspect ratio
    const float window_aspect_ratio = (float)window_width / (float)window_height;

    // Ensure that HD texture exists
    if (hd_texture == NULL) {
      create_hd_texture(); // Create the texture dynamically based on window size
    }

    if (!SDL_SetRenderTarget(rend, hd_texture)) {
      SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Couldn't set HD render target: %s", SDL_GetError());
    }

    // Fill HD texture with BG color
    SDL_SetRenderDrawColor(rend, global_background_color.r, global_background_color.g,
                           global_background_color.b, global_background_color.a);

    if (!SDL_RenderClear(rend)) {
      SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Couldn't clear HD texture: %s", SDL_GetError());
    }

    // Render the main texture to hd_texture. It has the same aspect ratio, so a NULL rect works.
    if (!SDL_RenderTexture(rend, main_texture, NULL, NULL)) {
      SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Couldn't render main texture to HD texture: %s",
                      SDL_GetError());
    };

    // Switch the render target back to the window
    if (!SDL_SetRenderTarget(rend, NULL)) {
      SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Couldn't reset render target to window: %s",
                      SDL_GetError());
    }

    float texture_width_hd, texture_height_hd;
    SDL_GetTextureSize(hd_texture, &texture_width_hd, &texture_height_hd);

    SDL_FRect dest_rect;

    if (window_aspect_ratio > texture_aspect_ratio) {
      // Window is relatively wider than the texture
      dest_rect.h = (float)window_height;
      dest_rect.w = dest_rect.h * texture_aspect_ratio;
      dest_rect.x = ((float)window_width - dest_rect.w) / 2.0f;
      dest_rect.y = 0;
      SDL_RenderTexture(rend, hd_texture, NULL, &dest_rect);
    } else if (window_aspect_ratio < texture_aspect_ratio) {
      // Window is relatively taller than the texture
      dest_rect.w = (float)window_width;
      dest_rect.h = dest_rect.w / texture_aspect_ratio;
      dest_rect.x = 0;
      dest_rect.y = ((float)window_height - dest_rect.h) / 2.0f;
      // Render the HD texture with the calculated destination rectangle
      SDL_RenderTexture(rend, hd_texture, NULL, &dest_rect);
    } else {
      // Window and texture aspect ratios match
      SDL_RenderTexture(rend, hd_texture, NULL, NULL);
    }
  }

  if (!SDL_RenderPresent(rend)) {
    SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Couldn't present renderer: %s", SDL_GetError());
  }

  if (!SDL_SetRenderTarget(rend, main_texture)) {
    SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Couldn't set renderer target to texture: %s",
                    SDL_GetError());
  }

  log_fps_stats();
}

int screensaver_init(void) {
  if (screensaver_initialized) {
    return 1;
  }
  SDL_SetRenderTarget(rend, main_texture);
  renderer_set_font_mode(1);
  global_background_color.r = 0, global_background_color.g = 0, global_background_color.b = 0;
  fx_cube_init(rend, (SDL_Color){255, 255, 255, 255}, texture_width, texture_height,
               fonts[font_mode]->glyph_x);
  SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Screensaver initialized");
  screensaver_initialized = 1;
  return 1;
}

void screensaver_draw(void) { dirty = fx_cube_update(); }

void screensaver_destroy(void) {
  fx_cube_destroy();
  renderer_set_font_mode(0);
  SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Screensaver destroyed");
  screensaver_initialized = 0;
}

void renderer_fix_texture_scaling_after_window_resize(config_params_s *conf) {
  SDL_SetRenderTarget(rend, NULL);
  if (conf->integer_scaling) {
    // SDL internal integer scaling works well for this purpose
    SDL_SetRenderLogicalPresentation(rend, texture_width, texture_height,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
  } else {
    if (hd_texture != NULL) {
      create_hd_texture(); // Recreate hd texture if necessary
    }
    setup_hd_texture_scaling();
  }
  SDL_SetTextureScaleMode(main_texture, texture_scaling_mode);
}

void show_error_message(const char *message) {
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "m8c error", message, win);
}

void renderer_clear_screen(void) {
  SDL_SetRenderDrawColor(rend, global_background_color.r, global_background_color.g,
                         global_background_color.b, global_background_color.a);
  SDL_SetRenderTarget(rend, main_texture);
  SDL_RenderClear(rend);
  SDL_SetRenderTarget(rend, NULL);

  SDL_RenderClear(rend);
}