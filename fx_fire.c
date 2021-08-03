#include "fx_fire.h"
#include "SDL_render.h"
#include <stdbool.h>
#include <stdio.h>

// by Hans Wennborg, https://www.hanshq.net/fire.html#sdl

#define WIDTH 160
#define HEIGHT 120

static const uint32_t palette[256] = {
/* Jare's original FirePal. */
#define C(r,g,b) (((r+b+g)) << 24 | (((r) * 4) << 16) | ((g) * 4 << 8) | ((b) * 4))
C( 0,   0,   0), C( 0,   1,   1), C( 0,   4,   5), C( 0,   7,   9),
C( 0,   8,  11), C( 0,   9,  12), C(15,   6,   8), C(25,   4,   4),
C(33,   3,   3), C(40,   2,   2), C(48,   2,   2), C(55,   1,   1),
C(63,   0,   0), C(63,   0,   0), C(63,   3,   0), C(63,   7,   0),
C(63,  10,   0), C(63,  13,   0), C(63,  16,   0), C(63,  20,   0),
C(63,  23,   0), C(63,  26,   0), C(63,  29,   0), C(63,  33,   0),
C(63,  36,   0), C(63,  39,   0), C(63,  39,   0), C(63,  40,   0),
C(63,  40,   0), C(63,  41,   0), C(63,  42,   0), C(63,  42,   0),
C(63,  43,   0), C(63,  44,   0), C(63,  44,   0), C(63,  45,   0),
C(63,  45,   0), C(63,  46,   0), C(63,  47,   0), C(63,  47,   0),
C(63,  48,   0), C(63,  49,   0), C(63,  49,   0), C(63,  50,   0),
C(63,  51,   0), C(63,  51,   0), C(63,  52,   0), C(63,  53,   0),
C(63,  53,   0), C(63,  54,   0), C(63,  55,   0), C(63,  55,   0),
C(63,  56,   0), C(63,  57,   0), C(63,  57,   0), C(63,  58,   0),
C(63,  58,   0), C(63,  59,   0), C(63,  60,   0), C(63,  60,   0),
C(63,  61,   0), C(63,  62,   0), C(63,  62,   0), C(63,  63,   0),
/* Followed by "white heat". */
#define W C(62,62,62)
W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W,
W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W,
W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W,
W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W,
W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W,
W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W,
W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W,
W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W,

#undef W
#undef C
};

static uint8_t fire[WIDTH * HEIGHT];
static uint8_t prev_fire[WIDTH * HEIGHT];
uint32_t *framebuf;
SDL_Texture *fx_texture;
SDL_Renderer *rend;

void fx_fire_init(SDL_Texture *output_texture) {
  fx_texture = output_texture;
  framebuf = malloc(2 * sizeof(uint32_t) * WIDTH * HEIGHT);
}

void fx_fire_destroy() { free(framebuf); }

void fx_fire_render() {

  int i;
  uint32_t sum;
  uint8_t avg;

  for (i = WIDTH + 1; i < (HEIGHT - 1) * WIDTH - 1; i++) {
    /* Average the eight neighbours. */
    sum = prev_fire[i - WIDTH - 1] + prev_fire[i - WIDTH] +
          prev_fire[i - WIDTH + 1] + prev_fire[i - 1] + prev_fire[i + 1] +
          prev_fire[i + WIDTH - 1] + prev_fire[i + WIDTH] +
          prev_fire[i + WIDTH + 1];
    avg = (uint8_t)(sum / 8);

    /* "Cool" the pixel if the two bottom bits of the
       sum are clear (somewhat random). For the bottom
       rows, cooling can overflow, causing "sparks". */
    if (!(sum & 3) && (avg > 0 || i >= (HEIGHT - 4) * WIDTH)) {
      avg--;
    }
    fire[i] = avg;
  }

  /* Copy back and scroll up one row.
     The bottom row is all zeros, so it can be skipped. */
  for (i = 0; i < (HEIGHT - 2) * WIDTH; i++) {
    prev_fire[i] = fire[i + WIDTH];
  }

  /* Remove dark pixels from the bottom rows (except again the
     bottom row which is all zeros). */
  for (i = (HEIGHT - 7) * WIDTH; i < (HEIGHT - 1) * WIDTH; i++) {
    if (fire[i] < 15) {
      fire[i] = 22 - fire[i];
    }
  }

  int framebuf_pos = 0;
  /* Copy to framebuffer and map to RGBA, scrolling up one row. */
  for (i = 0; i < (HEIGHT - 2) * WIDTH; i++) {
    const int color = palette[fire[i + WIDTH]];
    framebuf[framebuf_pos++] = color;
    framebuf[framebuf_pos++] = color;
  }

  /* Update the texture and render it. */
  SDL_UpdateTexture(fx_texture, NULL, framebuf, WIDTH * sizeof(framebuf[0]));
}