// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include "render.h"

#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>

#include "SDL2_inprint.h"
#include "command.h"
#include "fx_cube.h"
#include "fx_gradient.h"
#include "fx_piano.h"
#include "fx_tunnel.h"
#include "fxhelpers.h"
#include "gl_functions.h"

SDL_Window *win;
SDL_Renderer *rend;
SDL_Texture *main_texture;
SDL_Texture *m8_texture;
SDL_Texture *fx_texture;

int program_index = 3;
GLuint program_ids[3];

// Device state information
SDL_Color background_color = (SDL_Color){0, 0, 0, 0};
SDL_Color foreground_color = (SDL_Color){0, 0, 0, 0};
SDL_Color title_color = (SDL_Color){0, 0, 0, 0};
struct active_notes active_notes[8] = {0};
int vu_meter[2] = {0};

static uint32_t ticks;
static uint32_t ticks_fps;
static int fps;
uint8_t fullscreen = 0;
uint8_t special_fx = 0;
uint8_t enable_gl_shader = 0;

// Initializes SDL and creates a renderer and required surfaces
int initialize_sdl(int init_fullscreen) {

  ticks = SDL_GetTicks();

  const int window_width = 640*2;  // SDL window width
  const int window_height = 480*2; // SDL window height

  if (SDL_Init(SDL_INIT_EVERYTHING | SDL_VIDEO_OPENGL) != 0) {
    SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "SDL_Init: %s\n", SDL_GetError());
    return -1;
  }

    // OPENGL VERSION
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    // DOUBLE BUFFER
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);    

  win = SDL_CreateWindow("m8c", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         window_width, window_height,
                         SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL |
                             SDL_WINDOW_RESIZABLE | init_fullscreen);

  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

  rend = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

  SDL_RendererInfo renderer_info;
  SDL_GetRendererInfo(rend, &renderer_info);

  if (!strncmp(renderer_info.name, "opengl", 6)) {
    SDL_LogDebug(SDL_LOG_CATEGORY_RENDER, "OpenGL renderer found");
#ifndef __APPLE__
    // If you want to use GLEW or some other GL extension handler, do it here!
    if (!init_gl_extensions()) {
      SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Could not init GL extensions!");
      SDL_Quit();
      return -1;
    }
#endif

    program_ids[0] = compile_program("raster.vertex", "raster.fragment");
    program_ids[1] = compile_program("crt-pi.vertex", "crt-pi.fragment");
    program_ids[2] = compile_program("glitch.vertex", "glitch.fragment");
  }

  m8_texture = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_TARGET, 320, 240);

  fx_texture = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, 320, 240);

  main_texture = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_TARGET, 320, 240);

  SDL_SetTextureBlendMode(m8_texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureBlendMode(fx_texture, SDL_BLENDMODE_BLEND);

  SDL_SetRenderTarget(rend, fx_texture);
  SDL_SetRenderDrawColor(rend, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(rend);

  SDL_SetRenderTarget(rend, m8_texture);
  SDL_SetRenderDrawColor(rend, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
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
    break;
  case 4:
    fx_gradient_destroy();
    break;
  default:
    break;
  }
  special_fx = 0;

  // Destroy sdl stuff
  SDL_DestroyTexture(m8_texture);
  SDL_DestroyTexture(fx_texture);
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
    fx_piano_init(fx_texture);
    break;
  case 2:
    fx_piano_destroy();
    fx_tunnel_init(fx_texture, title_color);
    break;
  case 3:
    fx_tunnel_destroy();
    fx_cube_init(rend, title_color);
    break;
  case 4:
    fx_cube_destroy();
    fx_gradient_init(fx_texture, title_color);
    break;
  case 5:
    fx_gradient_destroy();
    special_fx = 0;
    break;
  default:
    break;
  }
  return special_fx;
}

int toggle_gl_shader() {
  program_index += 1;
  //query_gl_vars(program_ids[program_index]);
  if (program_index > 3) {
    program_index = 0;
    return 0;
  } else if (program_ids[program_index] > 0) return 1;
  return -1;
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
    break;
  case 4:
    fx_gradient_update();
    break;
  default:
    break;
  }
}

int draw_character(struct draw_character_command *command) {

  uint32_t fgcolor = (command->foreground.r << 16) |
                     (command->foreground.g << 8) | command->foreground.b;
  uint32_t bgcolor = (command->background.r << 16) |
                     (command->background.g << 8) | command->background.b;

  // SDL_Log("%s: %d %d", (char *)&command->c, command->pos.x, command->pos.y);

  /* Note characters appear between y=70 and y=140, update the active notes
    array */
  if (command->pos.y >= 70 && command->pos.y <= 140) {
    update_active_notes_data(command, active_notes);
  }

  /* Get the default color from tempo text, that should be pretty much always
   * visible */
  if (command->pos.x == 272 && command->pos.y == 50) {
    foreground_color = (SDL_Color){command->foreground.r, command->foreground.g,
                                   command->foreground.b, SDL_ALPHA_OPAQUE};
  }

  if (command->pos.x == 8 && command->pos.y == 30) {
    title_color = (SDL_Color){command->foreground.r, command->foreground.g,
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

  if (SDL_GetTicks() - ticks > 14) {
    ticks = SDL_GetTicks();
    SDL_SetRenderTarget(rend, main_texture);
    SDL_RenderCopy(rend, m8_texture, NULL, NULL);
    if (special_fx) {
      render_special_fx();
      SDL_RenderCopy(rend, fx_texture, NULL, NULL);
    }
    SDL_SetRenderTarget(rend, NULL);
    SDL_SetRenderDrawColor(rend, 0, 0, 0, 0);
    SDL_RenderClear(rend);

    SDL_RenderCopy(rend, main_texture, NULL, NULL);
    if (program_index < 3) {
      present_backbuffer(rend, win, main_texture, program_ids[program_index]);
    } else {
      SDL_RenderPresent(rend);
    }
    SDL_SetRenderTarget(rend, m8_texture);

    fps++;

    if (SDL_GetTicks() - ticks_fps > 5000) {
      ticks_fps = SDL_GetTicks();
      SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "%.1f fps\n", (float)fps / 5);
      fps = 0;
    }
  }
}
