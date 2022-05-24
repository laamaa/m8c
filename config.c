// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include "config.h"
#include "ini.h"
#include <SDL.h>
#include <assert.h>

/* Case insensitive string compare from ini.h library */
static int strcmpci(const char *a, const char *b) {
  for (;;) {
    int d = tolower(*a) - tolower(*b);
    if (d != 0 || !*a) {
      return d;
    }
    a++, b++;
  }
}

config_params_s init_config() {
  config_params_s c;

  c.filename = "config.ini"; // default config file to load

  c.init_fullscreen = 0;  // default fullscreen state at load
  c.init_use_gpu = 1;     // default to use hardware acceleration
  c.idle_ms = 10;         // default to high performance

  c.key_up = SDL_SCANCODE_UP;
  c.key_left = SDL_SCANCODE_LEFT;
  c.key_down = SDL_SCANCODE_DOWN;
  c.key_right = SDL_SCANCODE_RIGHT;
  c.key_select = SDL_SCANCODE_LSHIFT;
  c.key_select_alt = SDL_SCANCODE_A;
  c.key_start = SDL_SCANCODE_SPACE;
  c.key_start_alt = SDL_SCANCODE_S;
  c.key_opt = SDL_SCANCODE_LALT;
  c.key_opt_alt = SDL_SCANCODE_Z;
  c.key_edit = SDL_SCANCODE_LCTRL;
  c.key_edit_alt = SDL_SCANCODE_X;
  c.key_delete = SDL_SCANCODE_DELETE;
  c.key_reset = SDL_SCANCODE_R;

  c.gamepad_up = SDL_CONTROLLER_BUTTON_DPAD_UP;
  c.gamepad_left = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
  c.gamepad_down = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
  c.gamepad_right = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
  c.gamepad_select = SDL_CONTROLLER_BUTTON_BACK;
  c.gamepad_start = SDL_CONTROLLER_BUTTON_START;
  c.gamepad_opt = SDL_CONTROLLER_BUTTON_B;
  c.gamepad_edit = SDL_CONTROLLER_BUTTON_A;

  c.gamepad_analog_threshold = 32766;
  c.gamepad_analog_invert = 0;
  c.gamepad_analog_axis_updown = SDL_CONTROLLER_AXIS_LEFTY;
  c.gamepad_analog_axis_leftright = SDL_CONTROLLER_AXIS_LEFTX;
  c.gamepad_analog_axis_start = SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
  c.gamepad_analog_axis_select = SDL_CONTROLLER_AXIS_TRIGGERLEFT;
  c.gamepad_analog_axis_opt = SDL_CONTROLLER_AXIS_INVALID;
  c.gamepad_analog_axis_edit = SDL_CONTROLLER_AXIS_INVALID;

  return c;
}

// Write config to file
void write_config(config_params_s *conf) {

  // Open the default config file for writing
  char config_path[1024] = {0};
  sprintf(config_path, "%s%s", SDL_GetPrefPath("", "m8c"), conf->filename);
  SDL_RWops *rw = SDL_RWFromFile(config_path, "w");

  SDL_Log("Writing config file to %s", config_path);

  const unsigned int INI_LINE_COUNT = 36;

  // Entries for the config file
  char ini_values[INI_LINE_COUNT][50];
  int initPointer = 0;
  sprintf(ini_values[initPointer++], "[graphics]\n");
  sprintf(ini_values[initPointer++], "fullscreen=%s\n",
          conf->init_fullscreen ? "true" : "false");
  sprintf(ini_values[initPointer++], "use_gpu=%s\n",
          conf->init_use_gpu ? "true" : "false");
  sprintf(ini_values[initPointer++], "idle_ms=%d\n", conf->idle_ms);
  sprintf(ini_values[initPointer++], "[keyboard]\n");
  sprintf(ini_values[initPointer++], "key_up=%d\n", conf->key_up);
  sprintf(ini_values[initPointer++], "key_left=%d\n", conf->key_left);
  sprintf(ini_values[initPointer++], "key_down=%d\n", conf->key_down);
  sprintf(ini_values[initPointer++], "key_right=%d\n", conf->key_right);
  sprintf(ini_values[initPointer++], "key_select=%d\n", conf->key_select);
  sprintf(ini_values[initPointer++], "key_select_alt=%d\n", conf->key_select_alt);
  sprintf(ini_values[initPointer++], "key_start=%d\n", conf->key_start);
  sprintf(ini_values[initPointer++], "key_start_alt=%d\n", conf->key_start_alt);
  sprintf(ini_values[initPointer++], "key_opt=%d\n", conf->key_opt);
  sprintf(ini_values[initPointer++], "key_opt_alt=%d\n", conf->key_opt_alt);
  sprintf(ini_values[initPointer++], "key_edit=%d\n", conf->key_edit);
  sprintf(ini_values[initPointer++], "key_edit_alt=%d\n", conf->key_edit_alt);
  sprintf(ini_values[initPointer++], "key_delete=%d\n", conf->key_delete);
  sprintf(ini_values[initPointer++], "key_reset=%d\n", conf->key_reset);
  sprintf(ini_values[initPointer++], "[gamepad]\n");
  sprintf(ini_values[initPointer++], "gamepad_up=%d\n", conf->gamepad_up);
  sprintf(ini_values[initPointer++], "gamepad_left=%d\n", conf->gamepad_left);
  sprintf(ini_values[initPointer++], "gamepad_down=%d\n", conf->gamepad_down);
  sprintf(ini_values[initPointer++], "gamepad_right=%d\n", conf->gamepad_right);
  sprintf(ini_values[initPointer++], "gamepad_select=%d\n", conf->gamepad_select);
  sprintf(ini_values[initPointer++], "gamepad_start=%d\n", conf->gamepad_start);
  sprintf(ini_values[initPointer++], "gamepad_opt=%d\n", conf->gamepad_opt);
  sprintf(ini_values[initPointer++], "gamepad_edit=%d\n", conf->gamepad_edit);
  sprintf(ini_values[initPointer++], "gamepad_analog_threshold=%d\n",
          conf->gamepad_analog_threshold);
  sprintf(ini_values[initPointer++], "gamepad_analog_invert=%s\n",
          conf->gamepad_analog_invert ? "true" : "false");
  sprintf(ini_values[initPointer++], "gamepad_analog_axis_updown=%d\n",
          conf->gamepad_analog_axis_updown);
  sprintf(ini_values[initPointer++], "gamepad_analog_axis_leftright=%d\n",
          conf->gamepad_analog_axis_leftright);
  sprintf(ini_values[initPointer++], "gamepad_analog_axis_select=%d\n",
          conf->gamepad_analog_axis_select);
  sprintf(ini_values[initPointer++], "gamepad_analog_axis_start=%d\n",
          conf->gamepad_analog_axis_start);
  sprintf(ini_values[initPointer++], "gamepad_analog_axis_opt=%d\n",
          conf->gamepad_analog_axis_opt);
  sprintf(ini_values[initPointer++], "gamepad_analog_axis_edit=%d\n",
          conf->gamepad_analog_axis_edit);

  // Ensure we aren't writing off the end of the array
  assert(initPointer == INI_LINE_COUNT);

  if (rw != NULL) {
    // Write ini_values array to config file
    for (int i = 0; i < INI_LINE_COUNT; i++) {
      size_t len = SDL_strlen(ini_values[i]);
      if (SDL_RWwrite(rw, ini_values[i], 1, len) != len) {
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM,
                     "Couldn't write line into config file.");
      } else {
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Wrote to config: %s",
                     ini_values[i]);
      }
    }
    SDL_RWclose(rw);
  } else {
    SDL_Log("Couldn't write into config file.");
  }
}

// Read config
void read_config(config_params_s *conf) {

  char config_path[1024] = {0};
  sprintf(config_path, "%s%s", SDL_GetPrefPath("", "m8c"), conf->filename);
  SDL_Log("Reading config %s", config_path);
  ini_t *ini = ini_load(config_path);
  if (ini == NULL) {
    SDL_Log("Could not load config.");
    write_config(conf);
    return;
  }

  read_graphics_config(ini, conf);
  read_key_config(ini, conf);
  read_gamepad_config(ini, conf);

  // Frees the mem used for the config
  ini_free(ini);

  //Write any new default options after loading
  write_config(conf);
}

void read_graphics_config(ini_t *ini, config_params_s *conf) {
  const char *param_fs = ini_get(ini, "graphics", "fullscreen");
  const char *param_gpu = ini_get(ini, "graphics", "use_gpu");
  const char *idle_ms = ini_get(ini, "graphics", "idle_ms");

  if (strcmpci(param_fs, "true") == 0) {
    conf->init_fullscreen = 1;
  } else
    conf->init_fullscreen = 0;

  if(param_gpu != NULL){
    if (strcmpci(param_gpu, "true") == 0) {
      conf->init_use_gpu = 1;
    } else
      conf->init_use_gpu = 0;
  }

  if (idle_ms)
    conf->idle_ms = SDL_atoi(idle_ms);

}

void read_key_config(ini_t *ini, config_params_s *conf) {
  // TODO: Some form of validation

  const char *key_up = ini_get(ini, "keyboard", "key_up");
  const char *key_left = ini_get(ini, "keyboard", "key_left");
  const char *key_down = ini_get(ini, "keyboard", "key_down");
  const char *key_right = ini_get(ini, "keyboard", "key_right");
  const char *key_select = ini_get(ini, "keyboard", "key_select");
  const char *key_select_alt = ini_get(ini, "keyboard", "key_select_alt");
  const char *key_start = ini_get(ini, "keyboard", "key_start");
  const char *key_start_alt = ini_get(ini, "keyboard", "key_start_alt");
  const char *key_opt = ini_get(ini, "keyboard", "key_opt");
  const char *key_opt_alt = ini_get(ini, "keyboard", "key_opt_alt");
  const char *key_edit = ini_get(ini, "keyboard", "key_edit");
  const char *key_edit_alt = ini_get(ini, "keyboard", "key_edit_alt");
  const char *key_delete = ini_get(ini, "keyboard", "key_delete");
  const char *key_reset = ini_get(ini, "keyboard", "key_reset");

  if (key_up)
    conf->key_up = SDL_atoi(key_up);
  if (key_left)
    conf->key_left = SDL_atoi(key_left);
  if (key_down)
    conf->key_down = SDL_atoi(key_down);
  if (key_right)
    conf->key_right = SDL_atoi(key_right);
  if (key_select)
    conf->key_select = SDL_atoi(key_select);
  if (key_select_alt)
    conf->key_select_alt = SDL_atoi(key_select_alt);
  if (key_start)
    conf->key_start = SDL_atoi(key_start);
  if (key_start_alt)
    conf->key_start_alt = SDL_atoi(key_start_alt);
  if (key_opt)
    conf->key_opt = SDL_atoi(key_opt);
  if (key_opt_alt)
    conf->key_opt_alt = SDL_atoi(key_opt_alt);
  if (key_edit)
    conf->key_edit = SDL_atoi(key_edit);
  if (key_edit_alt)
    conf->key_edit_alt = SDL_atoi(key_edit_alt);
  if (key_delete)
    conf->key_delete = SDL_atoi(key_delete);
  if (key_reset)
    conf->key_reset = SDL_atoi(key_reset);
}

void read_gamepad_config(ini_t *ini, config_params_s *conf) {
  // TODO: Some form of validation

  const char *gamepad_up = ini_get(ini, "gamepad", "gamepad_up");
  const char *gamepad_left = ini_get(ini, "gamepad", "gamepad_left");
  const char *gamepad_down = ini_get(ini, "gamepad", "gamepad_down");
  const char *gamepad_right = ini_get(ini, "gamepad", "gamepad_right");
  const char *gamepad_select = ini_get(ini, "gamepad", "gamepad_select");
  const char *gamepad_start = ini_get(ini, "gamepad", "gamepad_start");
  const char *gamepad_opt = ini_get(ini, "gamepad", "gamepad_opt");
  const char *gamepad_edit = ini_get(ini, "gamepad", "gamepad_edit");
  const char *gamepad_analog_threshold =
      ini_get(ini, "gamepad", "gamepad_analog_threshold");
  const char *gamepad_analog_invert =
      ini_get(ini, "gamepad", "gamepad_analog_invert");
  const char *gamepad_analog_axis_updown =
      ini_get(ini, "gamepad", "gamepad_analog_axis_updown");
  const char *gamepad_analog_axis_leftright =
      ini_get(ini, "gamepad", "gamepad_analog_axis_leftright");
  const char *gamepad_analog_axis_select =
      ini_get(ini, "gamepad", "gamepad_analog_axis_select");
  const char *gamepad_analog_axis_start =
      ini_get(ini, "gamepad", "gamepad_analog_axis_start");
  const char *gamepad_analog_axis_opt =
      ini_get(ini, "gamepad", "gamepad_analog_axis_opt");
  const char *gamepad_analog_axis_edit =
      ini_get(ini, "gamepad", "gamepad_analog_axis_edit");

  if (gamepad_up)
    conf->gamepad_up = SDL_atoi(gamepad_up);
  if (gamepad_left)
    conf->gamepad_left = SDL_atoi(gamepad_left);
  if (gamepad_down)
    conf->gamepad_down = SDL_atoi(gamepad_down);
  if (gamepad_right)
    conf->gamepad_right = SDL_atoi(gamepad_right);
  if (gamepad_select)
    conf->gamepad_select = SDL_atoi(gamepad_select);
  if (gamepad_start)
    conf->gamepad_start = SDL_atoi(gamepad_start);
  if (gamepad_opt)
    conf->gamepad_opt = SDL_atoi(gamepad_opt);
  if (gamepad_edit)
    conf->gamepad_edit = SDL_atoi(gamepad_edit);
  if (gamepad_analog_threshold)
    conf->gamepad_analog_threshold = SDL_atoi(gamepad_analog_threshold);

  if (strcmpci(gamepad_analog_invert, "true") == 0)
    conf->gamepad_analog_invert = 1;
  else
    conf->gamepad_analog_invert = 0;

  if (gamepad_analog_axis_updown) conf->gamepad_analog_axis_updown = SDL_atoi(gamepad_analog_axis_updown);
  if (gamepad_analog_axis_leftright) conf->gamepad_analog_axis_leftright = SDL_atoi(gamepad_analog_axis_leftright);
  if (gamepad_analog_axis_select) conf->gamepad_analog_axis_select = SDL_atoi(gamepad_analog_axis_select);
  if (gamepad_analog_axis_start) conf->gamepad_analog_axis_start = SDL_atoi(gamepad_analog_axis_start);
  if (gamepad_analog_axis_opt) conf->gamepad_analog_axis_opt = SDL_atoi(gamepad_analog_axis_opt);
  if (gamepad_analog_axis_edit) conf->gamepad_analog_axis_edit = SDL_atoi(gamepad_analog_axis_edit);
}
