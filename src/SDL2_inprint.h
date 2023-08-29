// Bitmap font routine by driedfruit, https://github.com/driedfruit/SDL_inprint
// Released into public domain.
// Modified to support adding a background to text.

#ifndef SDL2_inprint_h
#define SDL2_inprint_h

#include <SDL.h>

extern void prepare_inline_font(unsigned char bits[],int font_width, int font_height);
extern void kill_inline_font(void);

extern void inrenderer(SDL_Renderer *renderer);
extern void infont(SDL_Texture *font);
extern void incolor1(SDL_Color *color);
extern void incolor(Uint32 color, Uint32 unused); /* Color must be in 0x00RRGGBB format ! */
extern void inprint(SDL_Renderer *dst, const char *str, Uint32 x, Uint32 y,
             Uint32 fgcolor, Uint32 bgcolor);

extern SDL_Texture *get_inline_font(void);

#endif /* SDL2_inprint_h */
