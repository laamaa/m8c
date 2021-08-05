#include "fx_tunnel.h"
#include "SDL_pixels.h"
#include "SDL_render.h"
#include "render.h"
#include <SDL2/SDL.h>

#define target_width 320
#define target_height 240
#define TEXTURE_SIZE 320
#define BIG_NUMBER 8388608

static int *texture;
static int *angle_table;
static int *depth_table;
static int *shade_table;
static uint32_t *framebuffer;
SDL_Texture *fx_texture;


void initialize_texture() {
  // Generate a texture simple square pattern
  texture = malloc(sizeof(int) * TEXTURE_SIZE * TEXTURE_SIZE);
  for (int y = 0; y < TEXTURE_SIZE; y++) {
    for (int x = 0; x < TEXTURE_SIZE; x++) {
      texture[x + (y * TEXTURE_SIZE)] =
          (x * 256 / TEXTURE_SIZE) ^ (y * 256 / TEXTURE_SIZE);
    }
  }
}

void initialize_lookup_tables() {
  angle_table = malloc(4 * sizeof(int) * target_width * target_height);
  depth_table = malloc(4 * sizeof(int) * target_width * target_height);
  shade_table = malloc(4 * sizeof(int) * target_width * target_height);

  for (int y = 0; y < target_height * 2; y++) {
    for (int x = 0; x < target_width * 2; x++) {

      double relative_x = x - target_width;
      double relative_y = target_height - y;
      int distance = relative_x * relative_x + relative_y * relative_y;
      double angle;

      // Prevent zero division by checking the relative y value
      if (relative_y == 0) {
        if (relative_x < 0) {
          angle = M_PI * -0.5;
        } else {
          angle = M_PI * 0.5;
        }
      } else {
        angle = SDL_atan(relative_x / relative_y);
        if (0 > relative_y) {
          angle += M_PI;
        }
      }

      // How many times should the texture repeat
      angle *= 6;

      // Convert radians to a value that can be used to look up stuff from the
      // texture
      angle = angle / (2 * M_PI) * TEXTURE_SIZE;

      while (0 > angle)
        angle += TEXTURE_SIZE;

      int array_pos = x + (y * target_width);
      angle_table[array_pos] = angle;

      // generate depth LUT
      int depth = distance == 0 ? BIG_NUMBER : BIG_NUMBER / distance;
      depth_table[array_pos] = depth;

      // generate brightness LUT
      double brightness = SDL_min(SDL_sqrt(distance), 255);
      shade_table[array_pos] = brightness;
    }
  }
}

void fx_tunnel_init(SDL_Texture *output_texture) {
  fx_texture = output_texture;
  // SDL_QueryTexture(fx_texture, NULL, NULL, &target_width, &target_height);
  framebuffer = malloc(sizeof(uint32_t) * target_width * target_height);
  initialize_texture();
  initialize_lookup_tables();
}

void fx_tunnel_destroy() {
  free(depth_table);
  free(angle_table);
  free(shade_table);
  free(texture);
  free(framebuffer);
}

void fx_tunnel_update() {
  static uint16_t rotation, zoom;
  int fb_array_pos = 0;
  int table_array_pos = 0;
  rotation += 3;
  zoom++;

  for (int y = 0; y < target_height; y++) {

    table_array_pos = y * target_width;
    for (int x = 0; x < target_width; x++) {
      int texture_x = angle_table[table_array_pos] + rotation;
      int texture_y = depth_table[table_array_pos] + zoom;

      while (texture_x < 0)
        texture_x += TEXTURE_SIZE;
      while (texture_y < 0)
        texture_y += TEXTURE_SIZE;
      while (texture_x > TEXTURE_SIZE)
        texture_x -= TEXTURE_SIZE;
      while (texture_y > TEXTURE_SIZE)
        texture_y -= TEXTURE_SIZE;
     
      uint32_t color =
          texture[(unsigned int)texture_x + (texture_y * TEXTURE_SIZE)] *
          shade_table[table_array_pos] / 255;

      color = (((0x30+color/4) << 24) | (color/2 << 16) | (color/2 << 8) | color);

      framebuffer[fb_array_pos] = color;

      table_array_pos++;
      fb_array_pos++;
    }
  }

  SDL_UpdateTexture(fx_texture, NULL, framebuffer,
                    target_width * sizeof(framebuffer[0]));
}
