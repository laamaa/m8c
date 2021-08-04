#ifndef FX_PIANO_H_
#define FX_PIANO_H_

#include "SDL_render.h"
#include <stdint.h>

void fx_piano_init(SDL_Texture *output_texture);
void fx_piano_update();
void fx_piano_destroy();

#endif