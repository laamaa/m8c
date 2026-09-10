// Copyright 2026 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef M8_COREMIDI_H
#define M8_COREMIDI_H

#ifdef M8C_COREMIDI_DISCOVERY

/* Find a physical M8 CoreMIDI source and destination.
 * Returns 1 when a pair was found, and writes their RtMidi port indices.
 */
int m8_coremidi_find_ports(int verbose, const char *preferred, unsigned int *input,
                           unsigned int *output);

int m8_coremidi_ports_still_exist(void);

#endif

#endif
