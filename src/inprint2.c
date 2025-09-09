// Bitmap font routine originally by driedfruit,
// https://github.com/driedfruit/SDL_inprint Released into public domain.
// Modified to support multiple fonts & adding a background to text.

#include "fonts/fonts.h"
#include <SDL3/SDL.h>

#define CHARACTERS_PER_ROW 94
#define CHARACTERS_PER_COLUMN 1

// Offset for seeking from limited character sets
static const int font_offset = 127 - CHARACTERS_PER_ROW * CHARACTERS_PER_COLUMN;

static SDL_Renderer *selected_renderer = NULL;
static SDL_Texture *inline_font = NULL;
static SDL_Texture *selected_font = NULL;
static const struct inline_font *selected_inline_font;
static Uint16 selected_font_w, selected_font_h;

void inline_font_initialize(const struct inline_font *font) {

  if (inline_font != NULL) {
    return;
  }

  selected_inline_font = font;
  selected_font_w = selected_inline_font->width;
  selected_font_h = selected_inline_font->height;

  SDL_IOStream *font_bmp =
      SDL_IOFromConstMem(selected_inline_font->image_data, selected_inline_font->image_size);

  SDL_Surface *surface = SDL_LoadBMP_IO(font_bmp, 1);

  // Black is transparent
  SDL_SetSurfaceColorKey(surface, true, SDL_MapSurfaceRGB(surface, 0, 0, 0));

  inline_font = SDL_CreateTextureFromSurface(selected_renderer, surface);

  SDL_DestroySurface(surface);

  selected_font = inline_font;
}

void inline_font_close(void) {
  SDL_DestroyTexture(inline_font);
  inline_font = NULL;
}

void inline_font_set_renderer(SDL_Renderer *renderer) { selected_renderer = renderer; }

void infont(SDL_Texture *font) {

  if (font == NULL) {
    return;
  }

  const int w =
      (int)SDL_GetNumberProperty(SDL_GetTextureProperties(font), SDL_PROP_TEXTURE_WIDTH_NUMBER, 0);
  const int h =
      (int)SDL_GetNumberProperty(SDL_GetTextureProperties(font), SDL_PROP_TEXTURE_HEIGHT_NUMBER, 0);

  selected_font = font;
  selected_font_w = w;
  selected_font_h = h;
}
void incolor1(const SDL_Color *color) {
  SDL_SetTextureColorMod(selected_font, color->r, color->g, color->b);
}
void incolor(const Uint32 fore) /* Color must be in 0x00RRGGBB format ! */
{
  SDL_Color pal[1];
  pal[0].r = (Uint8)((fore & 0x00FF0000) >> 16);
  pal[0].g = (Uint8)((fore & 0x0000FF00) >> 8);
  pal[0].b = (Uint8)(fore & 0x000000FF);
  SDL_SetTextureColorMod(selected_font, pal[0].r, pal[0].g, pal[0].b);
}
void inprint(SDL_Renderer *dst, const char *str, Uint32 x, Uint32 y, const Uint32 fgcolor,
             const Uint32 bgcolor) {
  SDL_FRect s_rect;
  SDL_FRect d_rect;
  SDL_FRect bg_rect;

  static uint32_t previous_fgcolor;

  d_rect.x = (float)x;
  d_rect.y = (float)y;
  s_rect.w = (float)selected_font_w / CHARACTERS_PER_ROW;
  s_rect.h = (float)selected_font_h / CHARACTERS_PER_COLUMN;
  d_rect.w = s_rect.w;
  d_rect.h = s_rect.h;

  if (dst == NULL)
    dst = selected_renderer;

  for (; *str; str++) {
    const int ascii_code = (int)*str;
    int id = ascii_code - font_offset;

#if (CHARACTERS_PER_COLUMN != 1)
    int row = id / CHARACTERS_PER_ROW;
    int col = id % CHARACTERS_PER_ROW;
    s_rect.x = col * s_rect.w;
    s_rect.y = row * s_rect.h;
#else
    s_rect.x = (float)id * s_rect.w;
    s_rect.y = 0;
#endif
    if (id + font_offset == '\n') {
      d_rect.x = (float)x;
      d_rect.y += s_rect.h + 1;
      continue;
    }
    if (fgcolor != previous_fgcolor) {
      incolor(fgcolor);
      previous_fgcolor = fgcolor;
    }

    if (bgcolor != fgcolor) {
      SDL_SetRenderDrawColor(selected_renderer, (bgcolor & 0x00FF0000) >> 16,
                             (bgcolor & 0x0000FF00) >> 8, bgcolor & 0x000000FF, 0xFF);
      bg_rect = d_rect;
      bg_rect.w = (float)selected_inline_font->glyph_x;
      bg_rect.h = (float)selected_inline_font->glyph_y;

      SDL_RenderFillRect(dst, &bg_rect);
    }
    // Do not try to render a whitespace character because the font doesn't have one
    if (ascii_code != 32) {
      SDL_RenderTexture(dst, selected_font, &s_rect, &d_rect);
    }
    d_rect.x += (float)selected_inline_font->glyph_x + 1;
  }
}

const struct inline_font *inline_font_get_current(void) {
  return selected_inline_font;
}
