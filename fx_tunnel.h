#ifndef FX_TUNNEL_H_
#define FX_TUNNEL_H_

#include "SDL_render.h"

void fx_tunnel_init(SDL_Texture *output_texture);
void fx_tunnel_destroy();
void fx_tunnel_update();

#endif