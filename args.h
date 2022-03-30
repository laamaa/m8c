// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef ARGS_H_
#define ARGS_H_

#include <SDL.h>

extern const char *argp_program_version;
extern const char *argp_program_bug_address;

/* Used by main to communicate with parse_opt. */
struct arguments
{
    char *ini_file;
};

SDL_LogPriority parseLogPriority(const char* arg);

/* Parse a single option. */
error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Our argp parser. */
extern struct argp argp;

//void setLogPriority (struct arguments *args);


void processArgs(int argc, char *argv[], struct arguments *arguments);

#endif