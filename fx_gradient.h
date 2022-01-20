#ifndef FX_GRADIENT_H_
#define FX_GRADIENT_H_

#include "SDL_render.h"
#include <stdint.h>

void fx_gradient_init(SDL_Texture *output_texture, SDL_Color mask_color);
void fx_gradient_update();
void fx_gradient_destroy();

#endif