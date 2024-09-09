//
// Created by jonne on 9/8/24.
//

#ifndef FX_PIANO_H
#define FX_PIANO_H

#include "SDL_render.h"
#include "command.h"

void fx_piano_init(SDL_Texture *output_texture);
void fx_piano_update();
void fx_piano_destroy();

void update_active_notes_data(struct draw_character_command *command, int font_mode);

#endif //FX_PIANO_H
