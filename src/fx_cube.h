#ifndef FX_CUBE_H_
#define FX_CUBE_H_

#include "SDL_render.h"
void fx_cube_init(SDL_Renderer *target_renderer, SDL_Color foreground_color,
                  unsigned int texture_width, unsigned int texture_height,
                  unsigned int font_glyph_width);
void fx_cube_destroy();
void fx_cube_update();
#endif