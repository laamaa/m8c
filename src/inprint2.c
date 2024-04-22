// Bitmap font routine originally by driedfruit,
// https://github.com/driedfruit/SDL_inprint Released into public domain.
// Modified to support multiple fonts & adding a background to text.

#include "inline_font.h"
#include <SDL.h>

#define CHARACTERS_PER_ROW 94
#define CHARACTERS_PER_COLUMN 1

// Offset for seeking from limited character sets
static const int font_offset =
    127 - (CHARACTERS_PER_ROW * CHARACTERS_PER_COLUMN);

static SDL_Renderer *selected_renderer = NULL;
static SDL_Texture *inline_font = NULL;
static SDL_Texture *selected_font = NULL;
static struct inline_font *selected_inline_font;
static Uint16 selected_font_w, selected_font_h;

void prepare_inline_font(struct inline_font *font) {
  SDL_Surface *surface;

  selected_font_w = font->width;
  selected_font_h = font->height;
  selected_inline_font = font;

  if (inline_font != NULL) {
    selected_font = inline_font;
    return;
  }

  SDL_RWops *font_bmp = SDL_RWFromConstMem(selected_inline_font->image_data,
                                           selected_inline_font->image_size);

  surface = SDL_LoadBMP_RW(font_bmp, 1);

  // Black is transparent
  SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, 0, 0, 0));

  inline_font = SDL_CreateTextureFromSurface(selected_renderer, surface);

  SDL_FreeSurface(surface);

  selected_font = inline_font;
}

void kill_inline_font(void) {
  SDL_DestroyTexture(inline_font);
  inline_font = NULL;
}

void inrenderer(SDL_Renderer *renderer) { selected_renderer = renderer; }

void infont(SDL_Texture *font) {
  Uint32 format;
  int access;
  int w, h;

  if (font == NULL) {
    return;
  }

  SDL_QueryTexture(font, &format, &access, &w, &h);

  selected_font = font;
  selected_font_w = w;
  selected_font_h = h;
}
void incolor1(SDL_Color *color) {
  SDL_SetTextureColorMod(selected_font, color->r, color->g, color->b);
}
void incolor(Uint32 fore,
             Uint32 unused) /* Color must be in 0x00RRGGBB format ! */
{
  SDL_Color pal[1];
  pal[0].r = (Uint8)((fore & 0x00FF0000) >> 16);
  pal[0].g = (Uint8)((fore & 0x0000FF00) >> 8);
  pal[0].b = (Uint8)((fore & 0x000000FF));
  SDL_SetTextureColorMod(selected_font, pal[0].r, pal[0].g, pal[0].b);
}
void inprint(SDL_Renderer *dst, const char *str, Uint32 x, Uint32 y,
             Uint32 fgcolor, Uint32 bgcolor) {
  SDL_Rect s_rect;
  SDL_Rect d_rect;
  SDL_Rect bg_rect;

  static uint32_t previous_fgcolor;

  d_rect.x = x;
  d_rect.y = y;
  s_rect.w = selected_font_w / CHARACTERS_PER_ROW;
  s_rect.h = selected_font_h / CHARACTERS_PER_COLUMN;
  d_rect.w = s_rect.w;
  d_rect.h = s_rect.h;

  if (dst == NULL)
    dst = selected_renderer;

  for (; *str; str++) {
    int id = (int)*str - font_offset;
#if (CHARACTERS_PER_COLUMN != 1)
    int row = id / CHARACTERS_PER_ROW;
    int col = id % CHARACTERS_PER_ROW;
    s_rect.x = col * s_rect.w;
    s_rect.y = row * s_rect.h;
#else
    s_rect.x = id * s_rect.w;
    s_rect.y = 0;
#endif
    if (id + font_offset == '\n') {
      d_rect.x = x;
      d_rect.y += s_rect.h + 1;
      continue;
    }
    if (fgcolor != previous_fgcolor) {
      incolor(fgcolor, 0);
      previous_fgcolor = fgcolor;
    }

    if (bgcolor != -1) {
      SDL_SetRenderDrawColor(selected_renderer,
                             (Uint8)((bgcolor & 0x00FF0000) >> 16),
                             (Uint8)((bgcolor & 0x0000FF00) >> 8),
                             (Uint8)((bgcolor & 0x000000FF)), 0xFF);
      bg_rect = d_rect;
      bg_rect.w = selected_inline_font->glyph_x;
      bg_rect.h = selected_inline_font->glyph_y;

      SDL_RenderFillRect(dst, &bg_rect);
    }
    SDL_RenderCopy(dst, selected_font, &s_rect, &d_rect);
    d_rect.x += selected_inline_font->glyph_x + 1;
  }
}
SDL_Texture *get_inline_font(void) { return selected_font; }
