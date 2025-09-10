#include "settings.h"

#include "SDL2_inprint.h"
#include "common.h"
#include <SDL3/SDL.h>
#include "fonts/fonts.h"

// Internal state
static int g_settings_open = 0;
static int g_selected_index = 0;
static int g_needs_redraw = 1;
static int g_config_saved = 0;
static SDL_Texture *g_settings_texture = NULL;


typedef enum capture_mode_t {
  CAPTURE_NONE,
  CAPTURE_KEY,
  CAPTURE_BUTTON,
  CAPTURE_AXIS
} capture_mode_t;

static capture_mode_t g_capture_mode = CAPTURE_NONE;
static void *g_capture_target = NULL; // points to conf field

typedef enum item_type_t {
  ITEM_HEADER,
  ITEM_TOGGLE_BOOL,
  ITEM_SAVE,
  ITEM_CLOSE,
  ITEM_SUBMENU,
  ITEM_BIND_KEY,
  ITEM_BIND_BUTTON,
  ITEM_BIND_AXIS,
  ITEM_INT_ADJ
} item_type_t;

typedef struct setting_item_s {
  const char *label;
  item_type_t type;
  void *target;
  int step;
  int min_val;
  int max_val;
} setting_item_s;

typedef enum settings_view_t { VIEW_ROOT, VIEW_KEYS, VIEW_GAMEPAD, VIEW_ANALOG } settings_view_t;
static settings_view_t g_view = VIEW_ROOT;
extern SDL_Gamepad *game_controllers[4];

static void add_item(setting_item_s *items, int *count, const char *label, item_type_t type,
                     void *target, int step, int min_val, int max_val) {
  items[*count] = (setting_item_s){label, type, target, step, min_val, max_val};
  (*count)++;
}

static void build_menu(const config_params_s *conf, setting_item_s *items, int *count) {
  *count = 0;
  switch (g_view) {
  case VIEW_ROOT:
    add_item(items, count, "Graphics       ", ITEM_HEADER, NULL, 0, 0, 0);
    add_item(items, count, "Integer scaling", ITEM_TOGGLE_BOOL, (void *)&conf->integer_scaling, 0,
             0, 0);
    // SDL apps are always full screen on iOS, hide the option
    if (TARGET_OS_IOS == 0) {
      add_item(items, count, "Fullscreen     ", ITEM_TOGGLE_BOOL, (void *)&conf->init_fullscreen, 0,
               0, 0);
    }
    add_item(items, count, "", ITEM_HEADER, NULL, 0, 0, 0);
    // Audio routing does not work on iOS, hide the items when building for that
    if (TARGET_OS_IOS == 0) {
      add_item(items, count, "Audio          ", ITEM_HEADER, NULL, 0, 0, 0);
      add_item(items, count, "Audio enabled  ", ITEM_TOGGLE_BOOL, (void *)&conf->audio_enabled, 0, 0,
               0);
      add_item(items, count, "", ITEM_HEADER, NULL, 0, 0, 0);
    }
    add_item(items, count, "Bindings       ", ITEM_HEADER, NULL, 0, 0, 0);
    add_item(items, count, "Keyboard bindings   >", ITEM_SUBMENU, NULL, 0, 0, 0);
    add_item(items, count, "Gamepad bindings    >", ITEM_SUBMENU, NULL, 0, 0, 0);
    add_item(items, count, "Gamepad analog axis >", ITEM_SUBMENU, NULL, 0, 0, 0);
    add_item(items, count, "", ITEM_HEADER, NULL, 0, 0, 0);
    add_item(items, count, "Save", ITEM_SAVE, NULL, 0, 0, 0);
    add_item(items, count, "Close", ITEM_CLOSE, NULL, 0, 0, 0);
    break;
  case VIEW_KEYS:
    add_item(items, count, "Keyboard bindings", ITEM_HEADER, NULL, 0, 0, 0);
    add_item(items, count, "Up          ", ITEM_BIND_KEY, (void *)&conf->key_up, 0, 0, 0);
    add_item(items, count, "Left        ", ITEM_BIND_KEY, (void *)&conf->key_left, 0, 0, 0);
    add_item(items, count, "Down        ", ITEM_BIND_KEY, (void *)&conf->key_down, 0, 0, 0);
    add_item(items, count, "Right       ", ITEM_BIND_KEY, (void *)&conf->key_right, 0, 0, 0);
    add_item(items, count, "Select      ", ITEM_BIND_KEY, (void *)&conf->key_select, 0, 0, 0);
    add_item(items, count, "Start       ", ITEM_BIND_KEY, (void *)&conf->key_start, 0, 0, 0);
    add_item(items, count, "Opt         ", ITEM_BIND_KEY, (void *)&conf->key_opt, 0, 0, 0);
    add_item(items, count, "Edit        ", ITEM_BIND_KEY, (void *)&conf->key_edit, 0, 0, 0);
    add_item(items, count, "Delete      ", ITEM_BIND_KEY, (void *)&conf->key_delete, 0, 0, 0);
    add_item(items, count, "Reset       ", ITEM_BIND_KEY, (void *)&conf->key_reset, 0, 0, 0);
    add_item(items, count, "Toggle audio", ITEM_BIND_KEY, (void *)&conf->key_toggle_audio, 0, 0, 0);
    add_item(items, count, "Toggle log  ", ITEM_BIND_KEY, (void *)&conf->key_toggle_log, 0, 0, 0);
    add_item(items, count, "", ITEM_HEADER, NULL, 0, 0, 0);
    add_item(items, count, "Back", ITEM_CLOSE, NULL, 0, 0, 0);
    break;
  case VIEW_GAMEPAD:
    add_item(items, count, "Gamepad bindings", ITEM_HEADER, NULL, 0, 0, 0);
    add_item(items, count, "Up    ", ITEM_BIND_BUTTON, (void *)&conf->gamepad_up, 0, 0, 0);
    add_item(items, count, "Left  ", ITEM_BIND_BUTTON, (void *)&conf->gamepad_left, 0, 0, 0);
    add_item(items, count, "Down  ", ITEM_BIND_BUTTON, (void *)&conf->gamepad_down, 0, 0, 0);
    add_item(items, count, "Right ", ITEM_BIND_BUTTON, (void *)&conf->gamepad_right, 0, 0, 0);
    add_item(items, count, "Select", ITEM_BIND_BUTTON, (void *)&conf->gamepad_select, 0, 0, 0);
    add_item(items, count, "Start ", ITEM_BIND_BUTTON, (void *)&conf->gamepad_start, 0, 0, 0);
    add_item(items, count, "Opt   ", ITEM_BIND_BUTTON, (void *)&conf->gamepad_opt, 0, 0, 0);
    add_item(items, count, "Edit  ", ITEM_BIND_BUTTON, (void *)&conf->gamepad_edit, 0, 0, 0);
    add_item(items, count, "Quit  ", ITEM_BIND_BUTTON, (void *)&conf->gamepad_quit, 0, 0, 0);
    add_item(items, count, "Reset ", ITEM_BIND_BUTTON, (void *)&conf->gamepad_reset, 0, 0, 0);
    add_item(items, count, "", ITEM_HEADER, NULL, 0, 0, 0);
    add_item(items, count, "Back", ITEM_CLOSE, NULL, 0, 0, 0);
    break;
  case VIEW_ANALOG:
    add_item(items, count, "Gamepad analog axis", ITEM_HEADER, NULL, 0, 0, 0);
    add_item(items, count, "Deadzone", ITEM_INT_ADJ, (void *)&conf->gamepad_analog_threshold, 1000,
             1000, 32767);
    add_item(items, count, "Axis Up/Down   ", ITEM_BIND_AXIS,
             (void *)&conf->gamepad_analog_axis_updown, 0, 0, 0);
    add_item(items, count, "Axis Left/Right", ITEM_BIND_AXIS,
             (void *)&conf->gamepad_analog_axis_leftright, 0, 0, 0);
    add_item(items, count, "Axis Select    ", ITEM_BIND_AXIS,
             (void *)&conf->gamepad_analog_axis_select, 0, 0, 0);
    add_item(items, count, "Axis Start     ", ITEM_BIND_AXIS,
             (void *)&conf->gamepad_analog_axis_start, 0, 0, 0);
    add_item(items, count, "Axis Opt       ", ITEM_BIND_AXIS,
             (void *)&conf->gamepad_analog_axis_opt, 0, 0, 0);
    add_item(items, count, "Axis Edit      ", ITEM_BIND_AXIS,
             (void *)&conf->gamepad_analog_axis_edit, 0, 0, 0);
    add_item(items, count, "", ITEM_HEADER, NULL, 0, 0, 0);
    add_item(items, count, "Back", ITEM_CLOSE, NULL, 0, 0, 0);
    break;
  }
}

static void settings_destroy_texture(SDL_Renderer *rend) {
  (void)rend;
  if (g_settings_texture != NULL) {
    SDL_DestroyTexture(g_settings_texture);
    g_settings_texture = NULL;
  }
}

void settings_toggle_open(void) {
  g_settings_open = !g_settings_open;
  g_selected_index = 1; // first actionable item
  g_capture_mode = CAPTURE_NONE;
  g_capture_target = NULL;
  g_needs_redraw = 1;
}

bool settings_is_open(void) { return g_settings_open != 0; }

static void settings_move(const config_params_s *conf, int delta) {
  if (!g_settings_open)
    return;

  setting_item_s items[64];
  int count = 0;
  build_menu(conf, items, &count);

  int idx = g_selected_index + delta;

  // Clamp within bounds
  if (idx < 0)
    idx = 0;
  if (idx >= count)
    idx = count - 1;

  // Skip headers in the direction of movement
  const int step = (delta >= 0) ? 1 : -1;
  while (idx >= 0 && idx < count && items[idx].type == ITEM_HEADER) {
    idx += step;
  }

  // Ensure we don't select before first actionable item
  if (idx < 1)
    idx = 1;

  // Final safety clamp
  if (idx >= count)
    idx = count - 1;

  g_selected_index = idx;
  g_needs_redraw = 1;
}

static void settings_activate(struct app_context *ctx, const setting_item_s *items, int count) {
  if (g_selected_index < 1 || g_selected_index >= count)
    return;
  const setting_item_s *it = &items[g_selected_index];
  config_params_s *conf = &ctx->conf;

  switch (it->type) {
  case ITEM_TOGGLE_BOOL: {
    unsigned int *val = it->target;
    *val = *val ? 0 : 1;
    if (it->target == &conf->init_fullscreen) {
      extern int toggle_fullscreen(config_params_s *config);
      toggle_fullscreen(conf);
    }
    if (it->target == &conf->integer_scaling) {
      extern void renderer_fix_texture_scaling_after_window_resize(config_params_s * config);
      renderer_fix_texture_scaling_after_window_resize(conf);
    }
    g_needs_redraw = 1;
    break;
  }
  case ITEM_SAVE: {
    write_config(conf);
    g_needs_redraw = 1;
    g_config_saved = 1;
    break;
  }
  case ITEM_CLOSE:
    settings_toggle_open();
    break;
  case ITEM_BIND_KEY:
    g_capture_mode = CAPTURE_KEY;
    g_capture_target = it->target;
    g_needs_redraw = 1;
    break;
  case ITEM_BIND_BUTTON:
    g_capture_mode = CAPTURE_BUTTON;
    g_capture_target = it->target;
    g_needs_redraw = 1;
    break;
  case ITEM_BIND_AXIS:
    g_capture_mode = CAPTURE_AXIS;
    g_capture_target = it->target;
    g_needs_redraw = 1;
    break;
  case ITEM_INT_ADJ:
  case ITEM_HEADER:
    break;
  default:
    break;
  }
}

void settings_handle_event(struct app_context *ctx, const SDL_Event *e) {
  if (!g_settings_open)
    return;

  if (e->type == SDL_EVENT_KEY_DOWN) {
    if (e->key.key == SDLK_ESCAPE || e->key.key == SDLK_F1) {
      if (g_capture_mode != CAPTURE_NONE) {
        g_capture_mode = CAPTURE_NONE;
        g_capture_target = NULL;
        g_needs_redraw = 1;
        return;
      }
      if (g_view != VIEW_ROOT) {
        g_view = VIEW_ROOT;
        g_selected_index = 1;
        g_needs_redraw = 1;
        return;
      }
      settings_toggle_open();
      return;
    }
    // Capture key remap
    if (g_capture_mode == CAPTURE_KEY) {
      if (g_capture_target != NULL) {
        unsigned int *dst = g_capture_target;
        *dst = e->key.scancode;
      }
      g_capture_mode = CAPTURE_NONE;
      g_capture_target = NULL;
      g_needs_redraw = 1;
      return;
    }
    if (e->key.key == SDLK_UP) {
      settings_move(&ctx->conf, -1);
      return;
    }
    if (e->key.key == SDLK_DOWN) {
      settings_move(&ctx->conf, 1);
      return;
    }
    if (e->key.key == SDLK_LEFT || e->key.key == SDLK_RIGHT) {
      setting_item_s items[64];
      int count = 0;
      build_menu(&ctx->conf, items, &count);
      if (g_selected_index > 0 && g_selected_index < count) {
        setting_item_s *it = &items[g_selected_index];
        if (it->type == ITEM_INT_ADJ && it->target != NULL) {
          int *val = (int *)it->target;
          int delta = (e->key.key == SDLK_LEFT ? -it->step : it->step);
          int newv = *val + delta;
          if (newv < it->min_val)
            newv = it->min_val;
          if (newv > it->max_val)
            newv = it->max_val;
          *val = newv;
          g_needs_redraw = 1;
          return;
        }
      }
    }
    if (e->key.key == SDLK_RETURN || e->key.key == SDLK_SPACE) {
      setting_item_s items[64];
      int count = 0;
      build_menu(&ctx->conf, items, &count);
      // Handle entering submenus from root based on item type
      if (g_view == VIEW_ROOT) {
        const setting_item_s *it = &items[g_selected_index];
        if (it->type == ITEM_SUBMENU && it->label &&
            SDL_strstr(it->label, "Keyboard bindings") == it->label) {
          g_view = VIEW_KEYS;
          g_selected_index = 1;
          g_needs_redraw = 1;
          return;
        }
        if (it->type == ITEM_SUBMENU && it->label &&
            SDL_strstr(it->label, "Gamepad bindings") == it->label) {
          g_view = VIEW_GAMEPAD;
          g_selected_index = 1;
          g_needs_redraw = 1;
          return;
        }
        if (it->type == ITEM_SUBMENU && it->label &&
            SDL_strstr(it->label, "Gamepad analog axis") == it->label) {
          g_view = VIEW_ANALOG;
          g_selected_index = 1;
          g_needs_redraw = 1;
          return;
        }
      }
      // Back item in submenus
      if (g_view != VIEW_ROOT) {
        const setting_item_s *it = &items[g_selected_index];
        if (it->type == ITEM_CLOSE && it->label && SDL_strcmp(it->label, "Back") == 0) {
          g_view = VIEW_ROOT;
          g_selected_index = 1;
          g_needs_redraw = 1;
          return;
        }
      }
      settings_activate(ctx, items, count);
      return;
    }
  }

  // Gamepad navigation and cancel/back handling
  if (e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
    SDL_GamepadButton btn = e->gbutton.button;

    // Cancel capture or go back/close with B/Back
    if (btn == SDL_GAMEPAD_BUTTON_EAST || btn == SDL_GAMEPAD_BUTTON_BACK) {
      if (g_capture_mode != CAPTURE_NONE) {
        g_capture_mode = CAPTURE_NONE;
        g_capture_target = NULL;
        g_needs_redraw = 1;
        return;
      }
      if (g_view != VIEW_ROOT) {
        g_view = VIEW_ROOT;
        g_selected_index = 1;
        g_needs_redraw = 1;
        return;
      }
      settings_toggle_open();
      return;
    }

    // If capturing a button, let the capture handler below process it
    if (g_capture_mode == CAPTURE_NONE) {
      // D-pad navigation
      if (btn == SDL_GAMEPAD_BUTTON_DPAD_UP) {
        settings_move(&ctx->conf, -1);
        return;
      }
      if (btn == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
        settings_move(&ctx->conf, 1);
        return;
      }
      if (btn == SDL_GAMEPAD_BUTTON_DPAD_LEFT || btn == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
        setting_item_s items[64];
        int count = 0;
        build_menu(&ctx->conf, items, &count);
        if (g_selected_index > 0 && g_selected_index < count) {
          setting_item_s *it = &items[g_selected_index];
          if (it->type == ITEM_INT_ADJ && it->target != NULL) {
            int *val = (int *)it->target;
            int delta = (btn == SDL_GAMEPAD_BUTTON_DPAD_LEFT ? -it->step : it->step);
            int newv = *val + delta;
            if (newv < it->min_val)
              newv = it->min_val;
            if (newv > it->max_val)
              newv = it->max_val;
            *val = newv;
            g_needs_redraw = 1;
            return;
          }
        }
      }
      // Activate/select with A
      if (btn == SDL_GAMEPAD_BUTTON_SOUTH) {
        setting_item_s items[64];
        int count = 0;
        build_menu(&ctx->conf, items, &count);
        // Handle entering submenus from root based on item type
        if (g_view == VIEW_ROOT) {
          const setting_item_s *it = &items[g_selected_index];
          if (it->type == ITEM_SUBMENU && it->label &&
              SDL_strstr(it->label, "Keyboard bindings") == it->label) {
            g_view = VIEW_KEYS;
            g_selected_index = 1;
            g_needs_redraw = 1;
            return;
          }
          if (it->type == ITEM_SUBMENU && it->label &&
              SDL_strstr(it->label, "Gamepad bindings") == it->label) {
            g_view = VIEW_GAMEPAD;
            g_selected_index = 1;
            g_needs_redraw = 1;
            return;
          }
          if (it->type == ITEM_SUBMENU && it->label &&
              SDL_strstr(it->label, "Gamepad analog axis") == it->label) {
            g_view = VIEW_ANALOG;
            g_selected_index = 1;
            g_needs_redraw = 1;
            return;
          }
        }
        // Back item in submenus
        if (g_view != VIEW_ROOT) {
          const setting_item_s *it = &items[g_selected_index];
          if (it->type == ITEM_CLOSE && it->label && SDL_strcmp(it->label, "Back") == 0) {
            g_view = VIEW_ROOT;
            g_selected_index = 1;
            g_needs_redraw = 1;
            return;
          }
        }
        settings_activate(ctx, items, count);
        return;
      }
    }
  }

  // Capture gamepad button
  if (g_capture_mode == CAPTURE_BUTTON && e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
    if (g_capture_target != NULL) {
      int *dst = (int *)g_capture_target;
      *dst = e->gbutton.button;
    }
    g_capture_mode = CAPTURE_NONE;
    g_capture_target = NULL;
    g_needs_redraw = 1;
    return;
  }
  // Capture axis on significant motion
  if (g_capture_mode == CAPTURE_AXIS && e->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
    if (SDL_abs(e->gaxis.value) > 16000) {
      if (g_capture_target != NULL) {
        int *dst = (int *)g_capture_target;
        *dst = e->gaxis.axis;
      }
      g_capture_mode = CAPTURE_NONE;
      g_capture_target = NULL;
      g_needs_redraw = 1;
      return;
    }
  }
}

void settings_render_overlay(SDL_Renderer *rend, const config_params_s *conf, int texture_w,
                             int texture_h) {
  if (!g_settings_open)
    return;

  const struct inline_font *previous_font = inline_font_get_current();
  if (previous_font->glyph_x != fonts_get(0)->glyph_x) {
    // Switch to small font if not active already
    inline_font_close();
    inline_font_initialize(fonts_get(0));
  }

  if (g_settings_texture == NULL) {
    g_settings_texture = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                                           texture_w, texture_h);
    if (g_settings_texture == NULL) {
      inline_font_close();
      inline_font_initialize(previous_font);
      return;
    }
    SDL_SetTextureBlendMode(g_settings_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(g_settings_texture, SDL_SCALEMODE_NEAREST);
    g_needs_redraw = 1;
  }

  if (!g_needs_redraw)
    goto composite;
  g_needs_redraw = 0;

  SDL_Texture *prev = SDL_GetRenderTarget(rend);
  SDL_SetRenderTarget(rend, g_settings_texture);

  // Background
  SDL_SetRenderDrawColor(rend, 0, 0, 0, 240);
  SDL_RenderClear(rend);

  // Title and items
  const Uint32 fg = 0xFFFFFF;
  const Uint32 bg = 0xFFFFFF;
  const Uint32 selected_item_fg = 0x00FFFF;
  const Uint32 selected_item_bg = 0x00FFFF;
  const Uint32 title = 0xFF0000;
  const Uint32 section_header = 0xAAAAFF;
  const int margin_x_unselected = fonts_get(0)->glyph_x+1;
  const int margin_x_selected = fonts_get(0)->glyph_x+1;
  int x = 8;
  int y = 8;

  inprint(rend, "M8C Config", x, y, title, title);
  y += 24;

  setting_item_s items[64];
  int count = 0;
  build_menu(conf, items, &count);
  if (g_selected_index >= count)
    g_selected_index = count - 1;

  if (g_capture_mode == CAPTURE_KEY) {
    inprint(rend, "Press a key... (Esc to cancel)", x, y, selected_item_fg, selected_item_fg);
  } else if (g_capture_mode == CAPTURE_BUTTON) {
    inprint(rend, "Press a gamepad button...", x, y, selected_item_fg, selected_item_fg);
  } else if (g_capture_mode == CAPTURE_AXIS) {
    inprint(rend, "Move a gamepad axis...", x, y, selected_item_fg, selected_item_fg);
  }
  if (g_config_saved) {
    inprint(rend, "Configuration saved", x, y, selected_item_fg, selected_item_fg);
    g_config_saved = 0;
  }
  y += 12;


  for (int i = 0; i < count; i++) {
    const int selected = (i == g_selected_index);
    const setting_item_s *it = &items[i];
    if (it->type == ITEM_HEADER) {
      inprint(rend, it->label, x, y, section_header, section_header);
      y += 12;
      continue;
    }
    if (it->type == ITEM_TOGGLE_BOOL) {
      const unsigned int *val = (const unsigned int *)it->target;
      char line[160];
      SDL_snprintf(line, sizeof line, "%s [%s]", it->label, (*val ? "On" : "Off"));
      inprint(rend, line, x + (selected ? margin_x_selected : margin_x_unselected), y,
              selected ? selected_item_fg : fg, selected ? selected_item_bg : bg);
      y += 12;
      continue;
    }
    if (it->type == ITEM_BIND_KEY) {
      int sc = *(int *)it->target;
      const char *name = SDL_GetScancodeName((SDL_Scancode)sc);
      if (name == NULL || name[0] == '\0') {
        name = "Unknown";
      }
      char line[160];
      SDL_snprintf(line, sizeof line, "%s: %s", it->label, name);
      inprint(rend, line, x + (selected ? margin_x_selected : margin_x_unselected), y,
              selected ? selected_item_fg : fg, selected ? selected_item_bg : bg);
      y += 12;
      continue;
    }
    if (it->type == ITEM_BIND_BUTTON) {
      int v = *(int *)it->target;
      const char *name = NULL;

      // Pick the first active controller if available
      SDL_Gamepad *pad = NULL;
      for (int gi = 0; gi < 4; gi++) {
        if (game_controllers[gi] != NULL) {
          pad = game_controllers[gi];
          break;
        }
      }

      if (pad) {
        // Use controller-specific face button labels when possible
        SDL_GamepadButtonLabel lbl = SDL_GetGamepadButtonLabel(pad, (SDL_GamepadButton)v);
        if (lbl != SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN) {
          name = SDL_GetGamepadStringForButton(v);
        }
      }
      // Generic fallback by standardized button name
      if (name == NULL || name[0] == '\0') {
        name = SDL_GetGamepadStringForButton((SDL_GamepadButton)v);
      }

      char line[160];
      if (name && name[0] != '\0') {
        SDL_snprintf(line, sizeof line, "%s: %s", it->label, name);
      } else {
        SDL_snprintf(line, sizeof line, "%s: %d", it->label, v);
      }
      inprint(rend, line, x + (selected ? margin_x_selected : margin_x_unselected), y,
              selected ? selected_item_fg : fg, selected ? selected_item_bg : bg);
      y += 12;
      continue;
    }
    if (it->type == ITEM_BIND_AXIS) {
      int v = *(int *)it->target;
      const char *name = SDL_GetGamepadStringForAxis((SDL_GamepadAxis)v);

      char line[160];
      if (name && name[0] != '\0') {
        SDL_snprintf(line, sizeof line, "%s: %s", it->label, name);
      } else {
        SDL_snprintf(line, sizeof line, "%s: %d", it->label, v);
      }
      inprint(rend, line, x + (selected ? margin_x_selected : margin_x_unselected), y,
              selected ? selected_item_fg : fg, selected ? selected_item_bg : bg);
      y += 12;
      continue;
    }
    if (it->type == ITEM_INT_ADJ) {
      char line[160];
      SDL_snprintf(line, sizeof line, "%s: %d  (Left/Right)", it->label, *(int *)it->target);
      inprint(rend, line, x + (selected ? margin_x_selected : margin_x_unselected), y,
              selected ? selected_item_fg : fg, selected ? selected_item_bg : bg);
      y += 12;
      continue;
    }
    if (it->type == ITEM_SAVE || it->type == ITEM_CLOSE) {
      char line[160];
      SDL_snprintf(line, sizeof line, "%s", it->label);
      inprint(rend, line, x + (selected ? margin_x_selected : margin_x_unselected), y,
              selected ? selected_item_fg : fg, selected ? selected_item_bg : bg);
      y += 12;
      continue;
    }
    if (it->type == ITEM_SUBMENU) {
      char line[160];
      SDL_snprintf(line, sizeof line, "%s", it->label);
      inprint(rend, line, x + (selected ? margin_x_selected : margin_x_unselected), y,
              selected ? selected_item_fg : fg, selected ? selected_item_bg : bg);
      y += 12;
    }
  }

  SDL_SetRenderTarget(rend, prev);

composite:
  SDL_RenderTexture(rend, g_settings_texture, NULL, NULL);
  if (previous_font->glyph_x != fonts_get(0)->glyph_x) {
    inline_font_close();
    inline_font_initialize(previous_font);
  }
}

// Handle renderer/size resets: drop the cached texture to recreate at new size on next frame
void settings_on_texture_size_change(SDL_Renderer *rend) {
  settings_destroy_texture(rend);
  g_needs_redraw = 1;
}
