#include "render.h"

#include "command.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

SDL_Window *win;
SDL_Renderer *rend;
TTF_Font *font;
SDL_Texture *glyph_texture;
SDL_Texture *background;
SDL_Surface *surface;
static uint32_t ticks;
#ifdef SHOW_FPS
static uint32_t ticks_fps;
static int fps;
#endif
const int font_size = 8;
uint8_t fullscreen = 0;

// Initializes SDL and creates a renderer and required surfaces
int initialize_sdl() {

  ticks = SDL_GetTicks();

  const int windowWidth = 640;  // SDL window width
  const int windowHeight = 480; // SDL window height

  // retutns zero on success else non-zero
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return -1;
  }

  if (TTF_Init() == -1) {
    printf("TTF_Init: %s\n", TTF_GetError());
    return -1;
  }

  win = SDL_CreateWindow("m8c", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         windowWidth, windowHeight,
                         SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL |
                             SDL_WINDOW_RESIZABLE);

  rend = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

  font = TTF_OpenFont("stealth57.ttf", font_size);

  surface =
      SDL_CreateRGBSurfaceWithFormat(0, 320, 240, 8, SDL_PIXELFORMAT_ARGB8888);
  if (surface == NULL) {
    SDL_Log("SDL_CreateRGBSurfaceWithFormat() failed: %s", SDL_GetError());
    exit(1);
  }

  SDL_SetRenderTarget(rend, background);

  return 1;
}

void close_renderer() {

  SDL_FreeSurface(surface);
  SDL_DestroyTexture(glyph_texture);
  SDL_DestroyTexture(background);
  TTF_CloseFont(font);
  SDL_DestroyRenderer(rend);
  SDL_DestroyWindow(win);
}

void toggle_fullscreen() {

  int fullscreen_state = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN;

  SDL_SetWindowFullscreen(win, fullscreen_state ? 0 : SDL_WINDOW_FULLSCREEN);
  SDL_ShowCursor(fullscreen_state);
}

int draw_character(struct draw_character_command *command) {

  SDL_Color fgcolor = {command->foreground.r, command->foreground.g,
                       command->foreground.b};

  SDL_Color bgcolor = {command->background.r, command->background.g,
                       command->background.b};

  SDL_Surface *char_surface;

  // Trash80: yeah I think I did something silly that if both are the same color
  // it means transparent background or something
  if (fgcolor.r == bgcolor.r && fgcolor.g == bgcolor.g &&
      fgcolor.b == bgcolor.b)
    char_surface = TTF_RenderUTF8_Solid(font, (char *)&command->c, fgcolor);
  else
    char_surface =
        TTF_RenderUTF8_Shaded(font, (char *)&command->c, fgcolor, bgcolor);

  if (!char_surface) {
    fprintf(stderr, "TTF_RenderUTF8_Solid error %s\n", TTF_GetError());
    return -1;
  }

  int texW, texH = 0;
  TTF_SizeUTF8(font, (char *)&command->c, &texW, &texH);
  SDL_Rect dstrect = {command->pos.x, command->pos.y + 3, texW, texH};

  SDL_BlitSurface(char_surface, NULL, surface, &dstrect);

  SDL_FreeSurface(char_surface);

  return 1;
}

void draw_rectangle(struct draw_rectangle_command *command) {

  SDL_Rect render_rect;

  render_rect.x = command->pos.x;
  render_rect.y = command->pos.y;
  render_rect.h = command->size.height;
  render_rect.w = command->size.width;

  uint32_t color = SDL_MapRGBA(surface->format, command->color.r,
                               command->color.g, command->color.b, 255);

  SDL_FillRect(surface, &render_rect, color);
}

// Direct pixel writing function by unwind, https://stackoverflow.com/a/20070273
void set_pixel(SDL_Surface *surface, int x, int y, Uint32 pixel) {

  Uint32 *const target_pixel =
      (Uint32 *)((Uint8 *)surface->pixels + y * surface->pitch +
                 x * surface->format->BytesPerPixel);
  *target_pixel = pixel;
}

void draw_waveform(struct draw_oscilloscope_waveform_command *command) {

  // clear the oscilloscope area
  memset(surface->pixels, 0, surface->w * 21 * surface->format->BytesPerPixel);

  // set oscilloscope color
  uint32_t color = SDL_MapRGBA(surface->format, command->color.r,
                               command->color.g, command->color.b, 255);

  for (int i = 0; i < command->waveform_size; i++) {
    // limit value because the oscilloscope commands seem to glitch occasionally
    if (command->waveform[i] > 20)
      command->waveform[i] = 20;
    // draw the pixels directly to the surface
    set_pixel(surface, i, command->waveform[i], color);
  }
}

void render_screen() {

  // process every 16ms (roughly 60fps)
  if (SDL_GetTicks() - ticks > 16) {

    ticks = SDL_GetTicks();

    SDL_RenderClear(rend);

    background = SDL_CreateTextureFromSurface(rend, surface);
    SDL_RenderCopy(rend, background, NULL, NULL);
    SDL_RenderPresent(rend);
    SDL_DestroyTexture(background);

#ifdef SHOW_FPS
    fps++;

    if (SDL_GetTicks() - ticks_fps > 5000) {
      ticks_fps = SDL_GetTicks();
      fprintf(stderr, "%d fps\n", fps / 5);
      fps = 0;
    }
#endif
  }
}