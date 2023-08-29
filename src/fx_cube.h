#ifndef FX_CUBE_H_
#define FX_CUBE_H_

#include "SDL_render.h"
void fx_cube_init(SDL_Renderer *target_renderer, SDL_Color foreground_color);
void fx_cube_destroy();
void fx_cube_update();
#endif