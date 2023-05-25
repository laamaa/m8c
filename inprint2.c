// Bitmap font routine originally by driedfruit,
// https://github.com/driedfruit/SDL_inprint Released into public domain.
// Modified to support multiple fonts & adding a background to text.

#include <SDL.h>

#define CHARACTERS_PER_ROW 16   /* I like 16 x 8 fontsets. */
#define CHARACTERS_PER_COLUMN 8 /* 128 x 1 is another popular format. */

static SDL_Renderer *selected_renderer = NULL;
static SDL_Texture *inline_font = NULL;
static SDL_Texture *selected_font = NULL;
static Uint16 selected_font_w, selected_font_h;

void prepare_inline_font(unsigned char bits[], int font_width,
                         int font_height) {
  Uint32 *pix_ptr, tmp;
  int i, len, j;
  SDL_Surface *surface;
  Uint32 colors[2];

  selected_font_w = font_width;
  selected_font_h = font_height;

  if (inline_font != NULL) {
    selected_font = inline_font;
    return;
  }

  surface = SDL_CreateRGBSurface(0, font_width, font_height, 32,
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
                                 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff
#else
                                 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
#endif
  );
  colors[0] = SDL_MapRGBA(surface->format, 0xFF, 0xFF, 0xFF, 0xFF);
  colors[1] = SDL_MapRGBA(surface->format, 0x00, 0x00, 0x00,
                          0x00 /* or 0xFF, to have bg-color */);

  /* Get pointer to pixels and array length */
  pix_ptr = (Uint32 *)surface->pixels;
  len = surface->h * surface->w / 8;

  /* Copy */
  for (i = 0; i < len; i++) {
    tmp = (Uint8)bits[i];
    for (j = 0; j < 8; j++) {
      Uint8 mask = (0x01 << j);
      pix_ptr[i * 8 + j] = colors[(tmp & mask) >> j];
    }
  }

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
    // prepare_inline_font();
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
    int id = (int)*str;
#if (CHARACTERS_PER_COLUMN != 1)
    int row = id / CHARACTERS_PER_ROW;
    int col = id % CHARACTERS_PER_ROW;
    s_rect.x = col * s_rect.w;
    s_rect.y = row * s_rect.h;
#else
    s_rect.x = id * s_rect.w;
    s_rect.y = 0;
#endif
    if (id == '\n') {
      d_rect.x = x;
      d_rect.y += s_rect.h;
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
      bg_rect.w = selected_font_w / CHARACTERS_PER_ROW - 1;
      // Silly hack to get big font background aligned correctly.
      if (bg_rect.h == 11) {
        bg_rect.y++;
      }

      SDL_RenderFillRect(dst, &bg_rect);
    }
    SDL_RenderCopy(dst, selected_font, &s_rect, &d_rect);
    d_rect.x += s_rect.w;
  }
}
SDL_Texture *get_inline_font(void) { return selected_font; }
