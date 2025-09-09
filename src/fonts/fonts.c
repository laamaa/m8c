// fonts.c
#include "fonts.h"
#include "font1.h"
#include "font2.h"
#include "font3.h"
#include "font4.h"
#include "font5.h"

#include <stddef.h>

static struct inline_font *fonts_storage[] = {
  &font_v1_small, &font_v1_large, &font_v2_small, &font_v2_large, &font_v2_huge
};

#define FONT_COUNT (sizeof(fonts_storage) / sizeof(fonts_storage[0]))

size_t fonts_count(void) {
  return FONT_COUNT;
}

const struct inline_font *fonts_get(const size_t index) {
  return (index < FONT_COUNT) ? (const struct inline_font *)fonts_storage[index] : NULL;
}

const struct inline_font *const *fonts_all(size_t *count) {
  if (count) *count = FONT_COUNT;
  // Cast to a read-only view so callers can’t mutate the array or elements
  return (const struct inline_font *const *)fonts_storage;
}