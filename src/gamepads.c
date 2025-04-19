//
// Created by jonne on 8/19/24.
//

#include "gamepads.h"

#include <SDL3/SDL.h>
#include <stdio.h>

// Maximum number of game controllers to support
#define MAX_CONTROLLERS 4

SDL_Gamepad *game_controllers[MAX_CONTROLLERS];

/**
 * Initializes available game controllers and loads game controller mappings.
 *
 * This function scans for connected joysticks and attempts to open those that are recognized
 * as game controllers. It also loads the game controller mapping database to improve compatibility
 * with various devices. Any errors during initialization are logged.
 *
 * @return The number of successfully initialized game controllers. Returns -1 if an error occurs
 *         during the initialization process.
 */
int gamepads_initialize() {

  int num_joysticks = 0;
  SDL_JoystickID *joystick_ids = NULL;

  if (SDL_Init(SDL_INIT_GAMEPAD) == false) {
    SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Failed to initialize SDL_GAMEPAD: %s", SDL_GetError());
    return -1;
  }

  int controller_index = 0;

  SDL_Log("Looking for game controllers");
  SDL_Delay(10); // Some controllers like XBone wired need a little while to get ready

  // Try to load the game controller database file
  char db_filename[2048] = {0};
  if (snprintf(db_filename, sizeof(db_filename), "%sgamecontrollerdb.txt",
               SDL_GetPrefPath("", "m8c")) >= (int)sizeof(db_filename)) {
    SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Path too long for buffer");
    return -1;
  }
  SDL_Log("Trying to open game controller database from %s", db_filename);
  SDL_IOStream *db_rw = SDL_IOFromFile(db_filename, "rb");
  if (db_rw == NULL) {
    snprintf(db_filename, sizeof(db_filename), "%sgamecontrollerdb.txt", SDL_GetBasePath());
    SDL_Log("Trying to open game controller database from %s", db_filename);
    db_rw = SDL_IOFromFile(db_filename, "rb");
  }

  if (db_rw != NULL) {
    const int mappings = SDL_AddGamepadMappingsFromIO(db_rw, true);
    if (mappings != -1) {
      SDL_Log("Found %d game controller mappings", mappings);
    } else {
      SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Error loading game controller mappings.");
    }
    SDL_CloseIO(db_rw);
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Unable to open game controller database file.");
  }

  joystick_ids = SDL_GetGamepads(&num_joysticks);
  if (joystick_ids == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Failed to get gamepad IDs: %s", SDL_GetError());
    return -1;
  }

  // Open all available game controllers
  SDL_Log("Found %d gamepads", num_joysticks);
  for (int i = 0; i < num_joysticks; i++) {
    if (!SDL_IsGamepad(joystick_ids[i]))
      continue;
    if (controller_index >= MAX_CONTROLLERS)
      break;
    game_controllers[controller_index] = SDL_OpenGamepad(joystick_ids[i]);
    if (game_controllers[controller_index] == NULL) {
      SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Failed to open gamepad %d: %s", i, SDL_GetError());
      continue;
    }
    SDL_Log("Controller %d: %s", controller_index + 1,
            SDL_GetGamepadName(game_controllers[controller_index]));
    controller_index++;
  }

  SDL_free(joystick_ids);

  return controller_index;
}

// Closes all open game controllers
void gamepads_close() {

  for (int i = 0; i < MAX_CONTROLLERS; i++) {
    if (game_controllers[i])
      SDL_CloseGamepad(game_controllers[i]);
  }

  SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}
