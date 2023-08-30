#include <SDL.h>
#include "SDL2_inprint.h"
#include "SDL_pixels.h"

#define target_width 320
#define target_height 240
static SDL_Texture *texture_cube;
static SDL_Texture *texture_text;
static SDL_Renderer *fx_renderer;
static SDL_Color line_color;

const char *text_m8c = "M8C";
const char *text_disconnected = "DEVICE DISCONNECTED";

static const float center_x = (float)target_width / 2;
static const float center_y = (float)target_height / 2;

static const float default_nodes[8][3] = {
    {-1, -1, -1}, {-1, -1, 1}, {-1, 1, -1}, {-1, 1, 1},
    {1, -1, -1},  {1, -1, 1},  {1, 1, -1},  {1, 1, 1}};

static int edges[12][2] = {{0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7},
                           {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

static float nodes[8][3];

static void scale(float factor0, float factor1, float factor2) {
  for (int i = 0; i < 8; i++) {
    nodes[i][0] *= factor0;
    nodes[i][1] *= factor1;
    nodes[i][2] *= factor2;
  }
}

static void rotate_cube(float angle_x, float angle_y) {
  float sin_x = SDL_sin(angle_x);
  float cos_x = SDL_cos(angle_x);
  float sin_y = SDL_sin(angle_y);
  float cos_y = SDL_cos(angle_y);
  for (int i = 0; i < 8; i++) {
    float x = nodes[i][0];
    float y = nodes[i][1];
    float z = nodes[i][2];

    nodes[i][0] = x * cos_x - z * sin_x;
    nodes[i][2] = z * cos_x + x * sin_x;

    z = nodes[i][2];

    nodes[i][1] = y * cos_y - z * sin_y;
    nodes[i][2] = z * cos_y + y * sin_y;
  }
}

void fx_cube_init(SDL_Renderer *target_renderer, SDL_Color foreground_color) {

  fx_renderer = target_renderer;
  line_color = foreground_color;

  texture_cube =
      SDL_CreateTexture(fx_renderer, SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_TARGET, target_width, target_height);
  texture_text =
      SDL_CreateTexture(fx_renderer, SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_TARGET, target_width, target_height);

  SDL_Texture *og_target = SDL_GetRenderTarget(fx_renderer);                        

  SDL_SetRenderTarget(fx_renderer, texture_text);
  SDL_SetRenderDrawColor(fx_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(fx_renderer);

  inprint(fx_renderer, text_disconnected, 150, 228, 0xFFFFFF, 0x000000);
  inprint(fx_renderer, text_m8c, 2, 2, 0xFFFFFF, 0x000000);

  SDL_SetRenderTarget(fx_renderer, og_target);

  // Initialize default nodes
  SDL_memcpy(nodes, default_nodes, sizeof(default_nodes));

  scale(50, 50, 50);
  rotate_cube(M_PI / 4, SDL_atan(SDL_sqrt(2)));

  SDL_SetTextureBlendMode(texture_cube, SDL_BLENDMODE_BLEND);
  SDL_SetTextureBlendMode(texture_text, SDL_BLENDMODE_BLEND);
}

void fx_cube_destroy() {
  SDL_DestroyTexture(texture_cube);
  SDL_DestroyTexture(texture_text);
}

void fx_cube_update() {
  SDL_Point points[24];
  int points_counter = 0;
  SDL_Texture *og_texture = SDL_GetRenderTarget(fx_renderer);

  SDL_SetRenderTarget(fx_renderer, texture_cube);
  SDL_SetRenderDrawColor(fx_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(fx_renderer);

  int seconds = SDL_GetTicks() / 1000;
  float scalefactor = 1 + (SDL_sin(seconds) * 0.01);

  scale(scalefactor, scalefactor, scalefactor);
  rotate_cube(M_PI / 180, M_PI / 270);

  for (int i = 0; i < 12; i++) {
    float *p1 = nodes[edges[i][0]];
    float *p2 = nodes[edges[i][1]];
    points[points_counter++] =
        (SDL_Point){p1[0] + center_x, nodes[edges[i][0]][1] + center_y};
    points[points_counter++] = (SDL_Point){p2[0] + center_x, p2[1] + center_y};
  }

  SDL_RenderCopy(fx_renderer, texture_text, NULL, NULL);
  SDL_SetRenderDrawColor(fx_renderer, line_color.r, line_color.g, line_color.b,
                         line_color.a);
  SDL_RenderDrawLines(fx_renderer, points, 24);

  SDL_SetRenderTarget(fx_renderer, og_texture);
  SDL_RenderCopy(fx_renderer, texture_cube, NULL, NULL);
}
