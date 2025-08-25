#include "SDL2_inprint.h"

#include <SDL3/SDL.h>
#include <math.h>

// Handle screensaver cube effect

static SDL_Texture *texture_cube;
static SDL_Texture *texture_text;
static SDL_Renderer *fx_renderer;
static SDL_Color line_color;

static unsigned int center_x = 320 / 2;
static unsigned int center_y = 240 / 2;

static const float default_nodes[8][3] = {{-1, -1, -1}, {-1, -1, 1}, {-1, 1, -1}, {-1, 1, 1},
                                          {1, -1, -1},  {1, -1, 1},  {1, 1, -1},  {1, 1, 1}};

static int edges[12][2] = {{0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7},
                           {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

static float nodes[8][3];

static void scale(const float factor0, const float factor1, const float factor2) {
  for (int i = 0; i < 8; i++) {
    nodes[i][0] *= factor0;
    nodes[i][1] *= factor1;
    nodes[i][2] *= factor2;
  }
}

static void rotate_cube(const float angle_x, const float angle_y) {
  const float sin_x = SDL_sinf(angle_x);
  const float cos_x = SDL_cosf(angle_x);
  const float sin_y = SDL_sinf(angle_y);
  const float cos_y = SDL_cosf(angle_y);
  for (int i = 0; i < 8; i++) {
    const float x = nodes[i][0];
    const float y = nodes[i][1];
    float z = nodes[i][2];

    nodes[i][0] = x * cos_x - z * sin_x;
    nodes[i][2] = z * cos_x + x * sin_x;

    z = nodes[i][2];

    nodes[i][1] = y * cos_y - z * sin_y;
    nodes[i][2] = z * cos_y + y * sin_y;
  }
}

void fx_cube_init(SDL_Renderer *target_renderer, const SDL_Color foreground_color,
                  const unsigned int texture_width, const unsigned int texture_height,
                  const unsigned int font_glyph_width) {

  fx_renderer = target_renderer;
  line_color = foreground_color;
  SDL_Point texture_size;

  SDL_Texture *og_target = SDL_GetRenderTarget(fx_renderer);

  texture_size.x = (int)SDL_GetNumberProperty(SDL_GetTextureProperties(og_target),
                                              SDL_PROP_TEXTURE_WIDTH_NUMBER, 0);
  texture_size.y = (int)SDL_GetNumberProperty(SDL_GetTextureProperties(og_target),
                                              SDL_PROP_TEXTURE_HEIGHT_NUMBER, 0);

  texture_cube = SDL_CreateTexture(fx_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                                   texture_size.x, texture_size.y);
  texture_text = SDL_CreateTexture(fx_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                                   texture_size.x, texture_size.y);

  SDL_SetRenderTarget(fx_renderer, texture_text);
  SDL_SetRenderDrawColor(fx_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(fx_renderer);

  inprint(fx_renderer, "M8 DEVICE NOT DETECTED", texture_width - font_glyph_width * 22 - 23,
          texture_height - 12, 0xFFFFFF, 0x000000);
  inprint(fx_renderer, "M8C", 2, 2, 0xFFFFFF, 0x000000);

  SDL_SetRenderTarget(fx_renderer, og_target);

  // Initialize default nodes
  SDL_memcpy(nodes, default_nodes, sizeof(default_nodes));

  scale(50, 50, 50);
  rotate_cube(M_PI / 6, SDL_atanf(SDL_sqrtf(2)));

  SDL_SetTextureBlendMode(texture_cube, SDL_BLENDMODE_BLEND);
  SDL_SetTextureBlendMode(texture_text, SDL_BLENDMODE_BLEND);

  center_x = (int)(texture_size.x / 2.0);
  center_y = (int)(texture_size.y / 2.0);
}

void fx_cube_destroy(void) {
  // Free resources
  SDL_DestroyTexture(texture_cube);
  SDL_DestroyTexture(texture_text);

  // Force clear renderer
  SDL_SetRenderTarget(fx_renderer, NULL);
  SDL_SetRenderDrawColor(fx_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(fx_renderer);
}

// Update the cube texture every 16ms>. Returns 1 if cube was updated, 0 if no changes were made.
int fx_cube_update(void) {
  static Uint64 ticks_last_update = 0;

  if (SDL_GetTicks() - ticks_last_update >= 16) {
    ticks_last_update = SDL_GetTicks();
    SDL_FPoint points[24];
    int points_counter = 0;
    SDL_Texture *og_texture = SDL_GetRenderTarget(fx_renderer);

    SDL_SetRenderTarget(fx_renderer, texture_cube);
    SDL_SetRenderDrawColor(fx_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(fx_renderer);

    const Uint64 ms = SDL_GetTicks();
    const float t = (float)ms / 1000.0f;
    const float pulse = 1.0f + 0.25f * SDL_sinf(t);

    rotate_cube(M_PI / 180, M_PI / 270);

    for (int i = 0; i < 12; i++) {
      const float *p1 = nodes[edges[i][0]];
      const float *p2 = nodes[edges[i][1]];
      points[points_counter++] = (SDL_FPoint){p1[0] * pulse + center_x, p1[1] * pulse + center_y};
      points[points_counter++] = (SDL_FPoint){p2[0] * pulse + center_x, p2[1] * pulse + center_y};
    }

    SDL_RenderTexture(fx_renderer, texture_text, NULL, NULL);
    SDL_SetRenderDrawColor(fx_renderer, line_color.r, line_color.g, line_color.b, line_color.a);
    SDL_RenderLines(fx_renderer, points, 24);

    SDL_SetRenderTarget(fx_renderer, og_texture);
    SDL_RenderTexture(fx_renderer, texture_cube, NULL, NULL);
    return 1;
  }
  return 0;
}
