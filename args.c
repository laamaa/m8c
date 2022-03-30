// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include <SDL.h>
#include <argp.h>

#include "args.h"
#include "custom_log.h"


// These could be passed around in the `arguments` struct, but this functionality
// doesn't leave this file so we can hide it from everyone here.
static SDL_LogPriority lpGlobal;
static SDL_LogPriority lpApp;
static SDL_LogPriority lpSerial;
static SDL_LogPriority lpComms;
static SDL_LogPriority lpConfig;
static SDL_LogPriority lpRender;
static SDL_LogPriority lpInput;

const char *argp_program_version = GIT_VER;
const char *argp_program_bug_address =
  "https://github.com/laamaa/m8c/issues";

/* Program documentation. */
static char doc[] =
  "An open-source graphical console for the Dirtywave M8 by Jonne Kokkonen\n"
  "See https://github.com/laamaa/m8c/blob/main/LICENSE for license info.\v"
  "Log levels:\n"
  "\tVERBOSE\t\tAll log messages\n"
  "\tDEBUG\t\t.\n"
  "\tINFO\t\t.\n"
  "\tWARN\t\t.\n"
  "\tERROR\t\t.\n"
  "\tCRITICAL\tProgram termination messages only\n\n"
  "Note that you can mix -L and specific log modifiers. The specific modifiers\n"
  "will override the global setting for that log topic.";

/* A description of the arguments we accept. */
static char args_doc[] = "";

/* The options we understand. */
static struct argp_option options[] = {
  {0,0,0,0, "Global Log Options:", 6},
  // Note: The short versions of commands (aka -L) do not work with an equal sign
  // as a separator. The equal character is passed as part of the parameter!
  {"log",        'L', "LEVEL",      0,  "Set global log level"},
  {"verbose",    'v', 0,      0,  "Produce verbose output (-L VERBOSE)" },
  {"quiet",      'q', 0,      0,  "Don't produce any output (-L CRITICAL)" },
  {"silent",     's', 0,      OPTION_ALIAS },
  {0,0,0,0, "Log Category Options:", 7},
  {"log-app",    700, "LEVEL",      0,  "Set app log level"},
  {"log-serial", 701, "LEVEL",      0,  "Set serial log level"},
  {"log-comms",  702, "LEVEL",      0,  "Set M8 comms log level"},
  {"log-config", 703, "LEVEL",      0,  "Set config log level"},
  {"log-render", 704, "LEVEL",      0,  "Set render log level"},
  {"log-input",  705, "LEVEL",      0,  "Set input log level"},

  // Config option not yet implemented
  {"config",     'c', "FILE", OPTION_HIDDEN,
   "Select a config ini file that is different from default." },
   {0,0,0,0, "Informational Options:", -1},
  { 0 }
};

/* Case insensitive string compare */
static int strcmpci(const char *a, const char *b) {
  for (;;) {
    int d = tolower(*a) - tolower(*b);
    if (d != 0 || !*a) {
      return d;
    }
    a++, b++;
  }
}

SDL_LogPriority parseLogPriority(const char* arg) {
  if (arg==NULL) return SDL_NUM_LOG_PRIORITIES;
  else if (strcmpci(arg, "VERBOSE" )==0) return SDL_LOG_PRIORITY_VERBOSE;
  else if (strcmpci(arg, "DEBUG"   )==0) return SDL_LOG_PRIORITY_DEBUG;
  else if (strcmpci(arg, "INFO"    )==0) return SDL_LOG_PRIORITY_INFO;
  else if (strcmpci(arg, "WARN"    )==0) return SDL_LOG_PRIORITY_WARN;
  else if (strcmpci(arg, "ERROR"   )==0) return SDL_LOG_PRIORITY_ERROR;
  else if (strcmpci(arg, "CRITICAL")==0) return SDL_LOG_PRIORITY_CRITICAL;
  else return SDL_NUM_LOG_PRIORITIES;  // enum abuse!
}

/* Parse a single option. */
error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Get the input argument from argp_parse, which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;

  // do this every time to save some typing
  SDL_LogPriority arg_val = parseLogPriority(arg);

  switch (key)
    {
    case 'q': case 's':
      lpGlobal = SDL_LOG_PRIORITY_CRITICAL;
      break;
    case 'v':
      lpGlobal = SDL_LOG_PRIORITY_VERBOSE;
      break;
    case 'L':
      if (arg[0] == '=')
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION,
            "Hint: don't use '=' with short options! (Use a space)");
      if (arg_val == SDL_NUM_LOG_PRIORITIES) argp_usage(state);
      lpGlobal = arg_val;
      break;
    case 700:
      if (arg_val == SDL_NUM_LOG_PRIORITIES) argp_usage(state);
      lpApp = arg_val;
      break;
    case 701:
      if (arg_val == SDL_NUM_LOG_PRIORITIES) argp_usage(state);
      lpSerial = arg_val;
      break;
    case 702:
      if (arg_val == SDL_NUM_LOG_PRIORITIES) argp_usage(state);
      lpComms = arg_val;
      break;
    case 703:
      if (arg_val == SDL_NUM_LOG_PRIORITIES) argp_usage(state);
      lpConfig = arg_val;
      break;
    case 704:
      if (arg_val == SDL_NUM_LOG_PRIORITIES) argp_usage(state);
      lpRender = arg_val;
      break;
    case 705:
      if (arg_val == SDL_NUM_LOG_PRIORITIES) argp_usage(state);
        lpInput = arg_val;
        break;
    case 'o':
      arguments->ini_file = arg;
      break;

    case ARGP_KEY_ARG:
      if (state->arg_num >= 0)
        /* Too many arguments. */
        argp_usage (state);
      break;

    case ARGP_KEY_END:
      //if (state->arg_num < 2)
        /* Not enough arguments. */
      //  argp_usage (state);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

/* Our argp parser. */
struct argp argp = { options, parse_opt, args_doc, doc };

void setLogPriority () {
  SDL_LogSetAllPriority(lpGlobal);

  // Only set category-specific setting if it's been modified
  if (lpApp!=SDL_NUM_LOG_PRIORITIES)
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, lpApp);
  if (lpSerial!=SDL_NUM_LOG_PRIORITIES)
    SDL_LogSetPriority(M8C_LOG_SERIAL,               lpSerial);
  if (lpComms!=SDL_NUM_LOG_PRIORITIES)
    SDL_LogSetPriority(M8C_LOG_COMMS,                lpComms);
  if (lpConfig!=SDL_NUM_LOG_PRIORITIES)
    SDL_LogSetPriority(M8C_LOG_CONFIG,               lpConfig);
  if (lpRender!=SDL_NUM_LOG_PRIORITIES) {
    SDL_LogSetPriority(SDL_LOG_CATEGORY_RENDER,      lpRender);
    SDL_LogSetPriority(SDL_LOG_CATEGORY_VIDEO,      lpRender);
  }
  if (lpInput!=SDL_NUM_LOG_PRIORITIES)
    SDL_LogSetPriority(SDL_LOG_CATEGORY_INPUT,       lpInput);
}


void processArgs(int argc, char *argv[], struct arguments *arguments) {
      /* Default values. */
  arguments->ini_file = "-";

  // Default global log priority
  lpGlobal = SDL_LOG_PRIORITY_CRITICAL;

  // Use SDL_NUM_LOG_PRIORITIES to indicate an unset value
  lpApp = SDL_NUM_LOG_PRIORITIES;
  lpSerial = SDL_NUM_LOG_PRIORITIES;
  lpComms = SDL_NUM_LOG_PRIORITIES;
  lpConfig = SDL_NUM_LOG_PRIORITIES;
  lpRender = SDL_NUM_LOG_PRIORITIES;

  argp_parse (&argp, argc, argv, 0, 0, arguments);

  setLogPriority();
}