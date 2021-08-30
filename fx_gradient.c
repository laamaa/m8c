#include "fx_gradient.h"
#include "SDL_pixels.h"
#include "render.h"

#include "SDL_render.h"
#include "SDL_stdinc.h"
#include "SDL_timer.h"
#include <math.h>

static int width = 320;
static int height = 240;
static SDL_Texture *fx_texture;
static SDL_Color mask;
static uint32_t *framebuffer;

static const uint32_t initcolor = 0xA0000000;

void fx_gradient_init(SDL_Texture *output_texture, SDL_Color mask_color) {
  fx_texture = output_texture;
  mask = mask_color;
  SDL_QueryTexture(fx_texture, NULL, NULL, &width, &height);
  framebuffer = malloc(sizeof(uint32_t) * width * height);
  SDL_memset4(framebuffer, initcolor, width * height);
}

void fx_gradient_update() {
  static uint32_t prev_ticks = 0;
  /* Calculating & drawing the gradient with CPU is a somewhat intensive
     operation, better to limit the framerate */
  if (SDL_GetTicks() - prev_ticks > 100) {
    const int gradient_speed = 1500; // lower number = faster
    float t = (float)SDL_GetTicks() / gradient_speed;
    for (int x = 0; x < width; x++) {
      for (int y = 0; y < height; y++) {
        int r, g, b;

        float pos = (float)x / (float)width + (1.0 - (float)y / (float)height);

        r = roundf((0.5 + 0.5 * SDL_cosf(t + pos)) * ((float)mask.r / 255) *
                  255);
        g = roundf((0.5 + 0.5 * SDL_cosf(t + pos + 2.0)) *
                  ((float)mask.g / 255) * 255);
        b = roundf((0.5 + 0.5 * SDL_cosf(t + pos + 4.0)) *
                  ((float)mask.b / 255) * 255);

        framebuffer[y * width + x] = (0x30 << 24) | (r << 16) | ((g << 8) | b);
      }
    }

    SDL_UpdateTexture(fx_texture, NULL, framebuffer,
                      width * sizeof(framebuffer[0]));
    prev_ticks = SDL_GetTicks();
  }
};

void fx_gradient_destroy() {
  SDL_free(framebuffer);
}