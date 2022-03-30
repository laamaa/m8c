#ifndef CUSTOMLOG_H_
#define CUSTOMLOG_H_

#include <SDL.h>

enum {
    // Logs pertaining to serial operations
    M8C_LOG_SERIAL = SDL_LOG_CATEGORY_CUSTOM,
    M8C_LOG_COMMS, // " M8 commands and parsing
    M8C_LOG_CONFIG // " config file processing
};

#endif