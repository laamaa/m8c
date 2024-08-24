//
// Created by jonne on 8/19/24.
//

#include "gamecontrollers.h"
#include "config.h"
#include "input.h"

#include <SDL.h>

static int num_joysticks = 0;
SDL_GameController *game_controllers[MAX_CONTROLLERS];

// Opens available game controllers and returns the amount of opened controllers
int gamecontrollers_initialize() {

  num_joysticks = SDL_NumJoysticks();
  int controller_index = 0;

  SDL_Log("Looking for game controllers");
  SDL_Delay(10); // Some controllers like XBone wired need a little while to get ready

  // Try to load the game controller database file
  char db_filename[1024] = {0};
  snprintf(db_filename, sizeof(db_filename), "%sgamecontrollerdb.txt", SDL_GetPrefPath("", "m8c"));
  SDL_Log("Trying to open game controller database from %s", db_filename);
  SDL_RWops *db_rw = SDL_RWFromFile(db_filename, "rb");
  if (db_rw == NULL) {
    snprintf(db_filename, sizeof(db_filename), "%sgamecontrollerdb.txt", SDL_GetBasePath());
    SDL_Log("Trying to open game controller database from %s", db_filename);
    db_rw = SDL_RWFromFile(db_filename, "rb");
  }

  if (db_rw != NULL) {
    const int mappings = SDL_GameControllerAddMappingsFromRW(db_rw, 1);
    if (mappings != -1)
      SDL_Log("Found %d game controller mappings", mappings);
    else
      SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Error loading game controller mappings.");
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Unable to open game controller database file.");
  }

  // Open all available game controllers
  for (int i = 0; i < num_joysticks; i++) {
    if (!SDL_IsGameController(i))
      continue;
    if (controller_index >= MAX_CONTROLLERS)
      break;
    game_controllers[controller_index] = SDL_GameControllerOpen(i);
    SDL_Log("Controller %d: %s", controller_index + 1,
            SDL_GameControllerName(game_controllers[controller_index]));
    controller_index++;
  }

  return controller_index;
}

// Closes all open game controllers
void gamecontrollers_close() {

  for (int i = 0; i < MAX_CONTROLLERS; i++) {
    if (game_controllers[i])
      SDL_GameControllerClose(game_controllers[i]);
  }
}

// Check whether a button is pressed on a gamepad and return 1 if pressed.
static int get_game_controller_button(const config_params_s *conf, SDL_GameController *controller,
                                      const int button) {

  const int button_mappings[8] = {conf->gamepad_up,     conf->gamepad_down, conf->gamepad_left,
                                  conf->gamepad_right,  conf->gamepad_opt,  conf->gamepad_edit,
                                  conf->gamepad_select, conf->gamepad_start};

  // Check digital buttons
  if (SDL_GameControllerGetButton(controller, button_mappings[button])) {
    return 1;
  }

  // If digital button isn't pressed, check the corresponding analog control
  switch (button) {
  case INPUT_UP:
    return SDL_GameControllerGetAxis(controller, conf->gamepad_analog_axis_updown) <
           -conf->gamepad_analog_threshold;
  case INPUT_DOWN:
    return SDL_GameControllerGetAxis(controller, conf->gamepad_analog_axis_updown) >
           conf->gamepad_analog_threshold;
  case INPUT_LEFT:
    return SDL_GameControllerGetAxis(controller, conf->gamepad_analog_axis_leftright) <
           -conf->gamepad_analog_threshold;
  case INPUT_RIGHT:
    return SDL_GameControllerGetAxis(controller, conf->gamepad_analog_axis_leftright) >
           conf->gamepad_analog_threshold;
  case INPUT_OPT:
    return SDL_GameControllerGetAxis(controller, conf->gamepad_analog_axis_opt) >
           conf->gamepad_analog_threshold;
  case INPUT_EDIT:
    return SDL_GameControllerGetAxis(controller, conf->gamepad_analog_axis_edit) >
           conf->gamepad_analog_threshold;
  case INPUT_SELECT:
    return SDL_GameControllerGetAxis(controller, conf->gamepad_analog_axis_select) >
           conf->gamepad_analog_threshold;
  case INPUT_START:
    return SDL_GameControllerGetAxis(controller, conf->gamepad_analog_axis_start) >
           conf->gamepad_analog_threshold;
  default:
    return 0;
  }
}

// Handle game controllers, simply check all buttons and analog axis on every
// cycle
int gamecontrollers_handle_buttons(const config_params_s *conf) {

  const int keycodes[8] = {key_up,  key_down, key_left,   key_right,
                           key_opt, key_edit, key_select, key_start};

  int key = 0;

  // Cycle through every active game controller
  for (int gc = 0; gc < num_joysticks; gc++) {
    // Cycle through all M8 buttons
    for (int button = 0; button < INPUT_MAX; button++) {
      // If the button is active, add the keycode to the variable containing
      // active keys
      if (get_game_controller_button(conf, game_controllers[gc], button)) {
        key |= keycodes[button];
      }
    }
  }

  return key;
}

input_msg_s gamecontrollers_handle_special_messages(const config_params_s *conf) {
  input_msg_s msg = {0};
  // Read special case game controller buttons quit and reset
  for (int gc = 0; gc < num_joysticks; gc++) {
    if (SDL_GameControllerGetButton(game_controllers[gc], conf->gamepad_quit) &&
        (SDL_GameControllerGetButton(game_controllers[gc], conf->gamepad_select) ||
         SDL_GameControllerGetAxis(game_controllers[gc], conf->gamepad_analog_axis_select)))
      msg = (input_msg_s){special, msg_quit, 0, 0};
    else if (SDL_GameControllerGetButton(game_controllers[gc], conf->gamepad_reset) &&
             (SDL_GameControllerGetButton(game_controllers[gc], conf->gamepad_select) ||
              SDL_GameControllerGetAxis(game_controllers[gc], conf->gamepad_analog_axis_select)))
      msg = (input_msg_s){special, msg_reset_display, 0, 0};
  }
  return msg;
}