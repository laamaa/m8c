//
// Created by jonne on 8/19/24.
//

#include "gamecontrollers.h"
#include "backends/m8.h"
#include "config.h"
#include "events.h"
#include "render.h"

#include <SDL3/SDL.h>
#include <stdio.h>

// Maximum number of game controllers to support
#define MAX_CONTROLLERS 4

static int num_joysticks = 0;
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
int gamecontrollers_initialize() {

  SDL_GetJoysticks(&num_joysticks);
  int controller_index = 0;

  SDL_Log("Looking for game controllers");
  SDL_Delay(10); // Some controllers like XBone wired need a little while to get ready

  // Try to load the game controller database file
  char db_filename[2048] = {0};
  if (snprintf(db_filename, sizeof(db_filename), "%sgamecontrollerdb.txt", SDL_GetPrefPath("", "m8c")) >= sizeof(db_filename)) {
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

  // Open all available game controllers
  for (int i = 0; i < num_joysticks; i++) {
    if (!SDL_IsGamepad(i))
      continue;
    if (controller_index >= MAX_CONTROLLERS)
      break;
    game_controllers[controller_index] = SDL_OpenGamepad(i);
    if (game_controllers[controller_index] == NULL) {
      SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Failed to open gamepad %d: %s", i, SDL_GetError());
      continue;
    }
    SDL_Log("Controller %d: %s", controller_index + 1,
            SDL_GetGamepadName(game_controllers[controller_index]));
    controller_index++;
  }

  return controller_index;
}

// Closes all open game controllers
void gamecontrollers_close() {

  for (int i = 0; i < MAX_CONTROLLERS; i++) {
    if (game_controllers[i])
      SDL_CloseGamepad(game_controllers[i]);
  }
}
