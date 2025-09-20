// Simple in-app settings overlay for configuring input bindings and a few options

#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <SDL3/SDL.h>
#include <stdbool.h>

#include "config.h"

// Forward declaration to avoid header coupling
struct app_context;

// Open/close and state query
void settings_toggle_open(void);
bool settings_is_open(void);

// Event handling (consume SDL events when open)
void settings_handle_event(struct app_context *ctx, const SDL_Event *e);

// Render the settings overlay into a texture-sized canvas and composite to the window
// texture_w/texture_h should be the logical render size (e.g. 320x240 or 480x320)
void settings_render_overlay(SDL_Renderer *rend, const config_params_s *conf, int texture_w, int texture_h);

// Notify settings overlay that logical render size changed; drops cached texture
void settings_on_texture_size_change(SDL_Renderer *rend);

#endif // SETTINGS_H_


