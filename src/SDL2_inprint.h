// Based on bitmap font routine by driedfruit, https://github.com/driedfruit/SDL_inprint
// Released into public domain.

#ifndef SDL2_inprint_h
#define SDL2_inprint_h

#include "fonts/inline_font.h"
#include <SDL3/SDL.h>

extern void inline_font_initialize(struct inline_font *font);
extern void inline_font_close(void);

extern void inline_font_set_renderer(SDL_Renderer *renderer);
extern void infont(SDL_Texture *font);
extern void incolor1(const SDL_Color *color);
extern void incolor(Uint32 color); /* Color must be in 0x00RRGGBB format ! */
extern void inprint(SDL_Renderer *dst, const char *str, Uint32 x, Uint32 y, Uint32 fgcolor,
                    Uint32 bgcolor);

extern SDL_Texture *get_inline_font(void);

#endif /* SDL2_inprint_h */
