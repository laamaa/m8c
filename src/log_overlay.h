// Copyright 2025 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef LOG_OVERLAY_H_
#define LOG_OVERLAY_H_

#include <SDL3/SDL.h>

// Initialize SDL log capture to mirror messages into the in-app overlay buffer
void log_overlay_init(void);

// Toggle overlay visibility
void log_overlay_toggle(void);

// Return non-zero if the overlay is currently visible
int log_overlay_is_visible(void);

// Invalidate any cached resources (e.g., after texture size change)
void log_overlay_invalidate(void);

// Destroy internal resources used by the overlay
void log_overlay_destroy(void);

// Ensure the overlay texture is up to date and composite it to the current render target
// font_mode_current is used to restore the caller's font after drawing
void log_overlay_render(SDL_Renderer *renderer,
			   int logical_texture_width,
			   int logical_texture_height,
			   SDL_ScaleMode texture_scaling_mode,
			   int font_mode_current);

#endif // LOG_OVERLAY_H_

