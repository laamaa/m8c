// Copyright 2025 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include "log_overlay.h"

#include <SDL3/SDL.h>

#include "SDL2_inprint.h"
#include "fonts/fonts.h"

#define LOG_BUFFER_MAX_LINES 512
#define LOG_LINE_MAX_CHARS 256

static SDL_Texture *overlay_texture = NULL;
static int overlay_visible = 0;
static int overlay_needs_redraw = 0;

static char log_lines[LOG_BUFFER_MAX_LINES][LOG_LINE_MAX_CHARS];
static int log_line_start = 0;
static int log_line_count = 0;

static SDL_LogOutputFunction prev_log_output_fn = NULL;
static void *prev_log_output_userdata = NULL;
static SDL_Mutex *log_mutex = NULL; // Mutex for protecting log buffer

static void log_buffer_append_line(const char *line) {
  if (line[0] == '\0') {
    return;
  }
  // Protect buffer updates (can be called from non-main threads)
  if (log_mutex)
    SDL_LockMutex(log_mutex);
  const int index = (log_line_start + log_line_count) % LOG_BUFFER_MAX_LINES;
  SDL_strlcpy(log_lines[index], line, LOG_LINE_MAX_CHARS);
  if (log_line_count < LOG_BUFFER_MAX_LINES) {
    log_line_count++;
  } else {
    log_line_start = (log_line_start + 1) % LOG_BUFFER_MAX_LINES;
  }
  overlay_needs_redraw = 1;
  if (log_mutex)
    SDL_UnlockMutex(log_mutex);
}

static void sdl_log_capture(void *userdata, int category, SDL_LogPriority priority,
                            const char *message) {
  // Suppress unused variable warnings
  (void)userdata;
  (void)category;
  (void)priority;

  char formatted[LOG_LINE_MAX_CHARS];
  SDL_snprintf(formatted, sizeof(formatted), ">%s", message ? message : "");
  log_buffer_append_line(formatted);

  if (prev_log_output_fn != NULL) {
    prev_log_output_fn(prev_log_output_userdata, category, priority, message);
  }
}

void log_overlay_init(void) {
  // Create synchronization primitive before hooking log output
  if (!log_mutex) {
    log_mutex = SDL_CreateMutex();
    if (!log_mutex) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create log mutex: %s", SDL_GetError());
    }
  }
  SDL_GetLogOutputFunction(&prev_log_output_fn, &prev_log_output_userdata);
  SDL_SetLogOutputFunction(sdl_log_capture, NULL);
}

void log_overlay_toggle(void) {
  overlay_visible = !overlay_visible;
  overlay_needs_redraw = 1;
}

int log_overlay_is_visible(void) { return overlay_visible; }

void log_overlay_invalidate(void) {
  if (overlay_texture != NULL) {
    SDL_DestroyTexture(overlay_texture);
    overlay_texture = NULL;
  }
  overlay_needs_redraw = 1;
}

void log_overlay_destroy(void) {
  // Restore previous log output function
  if (prev_log_output_fn) {
    SDL_SetLogOutputFunction(prev_log_output_fn, prev_log_output_userdata);
    prev_log_output_fn = NULL;
    prev_log_output_userdata = NULL;
  }
  if (overlay_texture != NULL) {
    SDL_DestroyTexture(overlay_texture);
    overlay_texture = NULL;
  }
  // Destroy synchronization primitive
  if (log_mutex) {
    SDL_DestroyMutex(log_mutex);
    log_mutex = NULL;
  }
  overlay_needs_redraw = 1;
}

void log_overlay_render(SDL_Renderer *renderer, int logical_texture_width,
                        int logical_texture_height, SDL_ScaleMode scale_mode,
                        int font_mode_current) {
  if (!overlay_visible) {
    return;
  }
  if (overlay_texture == NULL) {
    overlay_texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                          logical_texture_width, logical_texture_height);
    if (overlay_texture == NULL) {
      SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Couldn't create log texture: %s", SDL_GetError());
      return;
    }
    SDL_SetTextureBlendMode(overlay_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(overlay_texture, scale_mode);
  }

  // Only update the overlay texture when its contents changed.
  if (overlay_needs_redraw) {
    overlay_needs_redraw = 0;

    // Take a snapshot of the log ring buffer so we can render without holding the mutex.
    // This prevents data races and avoids deadlocks if rendering logs internally.
    int local_count = 0;
    int local_start = 0;
    if (log_mutex)
      SDL_LockMutex(log_mutex);
    local_count = log_line_count;
    local_start = log_line_start;

    // Snapshot holds copies of each visible line in chronological order.
    char(*snapshot)[LOG_LINE_MAX_CHARS] = NULL;
    if (local_count > 0) {
      snapshot = (char(*)[LOG_LINE_MAX_CHARS])SDL_calloc((size_t)local_count, sizeof(log_lines[0]));
    }
    if (snapshot) {
      for (int i = 0; i < local_count; i++) {
        const int idx = (local_start + i) % LOG_BUFFER_MAX_LINES;
        SDL_strlcpy(snapshot[i], log_lines[idx], LOG_LINE_MAX_CHARS);
      }
    }
    if (log_mutex)
      SDL_UnlockMutex(log_mutex);

    // Bind the overlay texture as the render target and clear it with a translucent background.
    SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);
    if (!SDL_SetRenderTarget(renderer, overlay_texture)) {
      SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to set render target: %s", SDL_GetError());
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220);
    SDL_RenderClear(renderer);

    // Switch to a small font for the overlay; remember previous mode to restore later.
    const int prev_font_mode = font_mode_current;
    inline_font_close();
    const struct inline_font *font_small = fonts_get(0);
    if (font_small) {
      inline_font_initialize(font_small);

      // Layout calculations:
      // - glyph_x/y = character cell size in pixels.
      // - margin_x/y = inner padding around the overlay.
      // - cols = how many characters fit per line (accounting for a 1px inter-glyph gap).
      const int line_height = font_small->glyph_y + 1;
      const int margin_x = 2;
      const int margin_y = 1;
      const int usable_width = logical_texture_width - (margin_x * 2);
      const int cols = SDL_max(1, usable_width / (font_small->glyph_x + 1));

      // Determine how many text rows fit on the screen vertically.
      const int max_rows = (logical_texture_height - margin_y * 2) / line_height;

      // We want to show the newest content; walk backwards over the snapshot to find
      // which line (and intra-line character offset) should be the first visible row,
      // so that the last max_rows rows are visible.
      int rows_needed = max_rows;
      int start_idx = 0;            // index in snapshot[] to start drawing from
      size_t start_char_offset = 0; // per-line character offset (for wrapped lines)

      if (local_count > 0) {
        for (int n = local_count - 1; n >= 0 && rows_needed > 0; n--) {
          if (!snapshot) break; // nullptr safety
          const size_t len = SDL_strlen(snapshot[n]);
          // How many wrapped rows this line consumes
          const int rows_for_line = SDL_max(1, (int)((len + cols - 1) / cols));
          if (rows_for_line >= rows_needed) {
            // This line provides the first visible portion.
            // Compute which character to start from so we only draw the last rows_needed rows.
            const int offset = SDL_max(0, (int)len - rows_needed * cols);
            start_idx = n;
            start_char_offset = (size_t)offset;
            break;
          }
          // Not enough rows on this line, include it fully and continue upwards.
          rows_needed -= rows_for_line;
          start_idx = n;
          start_char_offset = 0;
        }
      }

      // Render loop:
      // - Iterate from start_idx to the newest item (end of snapshot).
      // - For each line, draw it in chunks of `cols` characters (word-wrap by fixed width).
      // - Stop when we run out of vertical space.
      if (local_count > 0) {
        int y = margin_y;
        size_t offset = start_char_offset;

        for (int cur = start_idx; cur < local_count && y < logical_texture_height; cur++) {
          if (!snapshot || cur < 0 || cur >= local_count) {
            break;
          }

          const char *s = snapshot[cur];
          const size_t len = SDL_strlen(s);

          for (size_t pos = offset; pos < len && y < logical_texture_height;) {
            const Uint32 fg = 0xFFFFFF; // draw text in white
            const size_t remaining = len - pos;

            // Take up to `cols` characters for this visual row
            size_t take = (size_t)cols < remaining ? (size_t)cols : remaining;

            // Copy the slice into a temporary buffer for printing
            char buf[LOG_LINE_MAX_CHARS];
            if (take >= sizeof(buf)) {
              take = sizeof(buf) - 1;
            }
            SDL_memcpy(buf, s + pos, take);
            buf[take] = '\0';

            // Draw the row and advance one line vertically
            inprint(renderer, buf, margin_x, y, fg, fg);
            y += line_height;
            pos += take;
          }

          // After the first (possibly partial) slice of this line, subsequent lines start at 0
          offset = 0;
        }
      }
    } else {
      SDL_LogError(SDL_LOG_CATEGORY_RENDER, "fonts_get(0) returned NULL");
    }

    // Restore previous font mode and previous render target.
    inline_font_close();
    inline_font_initialize(fonts_get(prev_font_mode));
    SDL_SetRenderTarget(renderer, prev_target);

    // Free the snapshot after rendering.
    if (snapshot) {
      SDL_free(snapshot);
    }
  }

  // Composite the overlay texture to the current render target every frame while visible.
  if (overlay_texture) {
    if (!SDL_RenderTexture(renderer, overlay_texture, NULL, NULL)) {
      SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Couldn't render log overlay texture: %s",
                      SDL_GetError());
    }
  }
}
