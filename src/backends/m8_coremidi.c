// Copyright 2026 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifdef M8C_COREMIDI_DISCOVERY

#include "m8_coremidi.h"
#include <CoreMIDI/CoreMIDI.h>
#include <SDL3/SDL.h>

static MIDIEndpointRef selected_source, selected_destination;

static void get_name(MIDIObjectRef object, char *buffer, size_t size) {
  CFStringRef value = NULL;
  buffer[0] = '\0';
  if (object && MIDIObjectGetStringProperty(object, kMIDIPropertyName, &value) == noErr && value) {
    if (!CFStringGetCString(value, buffer, size, kCFStringEncodingUTF8))
      buffer[0] = '\0';
    CFRelease(value);
  }
}

static int inspect_source(MIDIEndpointRef source, const char *preferred,
                           MIDIEntityRef *entity, int *is_preferred) {
  MIDIDeviceRef device = 0;
  char source_name[256], entity_name[256], device_name[256];
  // Virtual sources and destinations have no owning entity.
  if (!source || MIDIEndpointGetEntity(source, entity) != noErr || !*entity ||
      MIDIEntityGetDevice(*entity, &device) != noErr || !device)
    return 0;
  get_name(source, source_name, sizeof(source_name));
  get_name(*entity, entity_name, sizeof(entity_name));
  get_name(device, device_name, sizeof(device_name));
  *is_preferred = preferred && (SDL_strcmp(preferred, source_name) == 0 ||
                               SDL_strcmp(preferred, entity_name) == 0 ||
                               SDL_strcmp(preferred, device_name) == 0);
  return SDL_strcasestr(source_name, "M8") || SDL_strcasestr(entity_name, "M8") ||
         SDL_strcasestr(device_name, "M8");
}

int m8_coremidi_find_ports(int verbose, const char *preferred, unsigned int *input,
                           unsigned int *output) {
  const ItemCount sources = MIDIGetNumberOfSources();
  const ItemCount destinations = MIDIGetNumberOfDestinations();
  int found = 0;
  for (ItemCount in = 0; in < sources; in++) {
    MIDIEndpointRef source = MIDIGetSource(in);
    MIDIEntityRef entity = 0;
    int is_preferred = 0;
    if (!inspect_source(source, preferred, &entity, &is_preferred))
      continue;
    for (ItemCount out = 0; out < destinations; out++) {
      MIDIEndpointRef destination = MIDIGetDestination(out);
      MIDIEntityRef output_entity = 0;
      if (!destination || MIDIEndpointGetEntity(destination, &output_entity) != noErr ||
          output_entity != entity)
        continue;
      if (!found || is_preferred) {
        *input = (unsigned int)in;
        *output = (unsigned int)out;
        selected_source = source;
        selected_destination = destination;
        found = 1;
      }
      if (is_preferred)
        goto done;
    }
  }
done:
  if (verbose && found)
    SDL_Log("Selected physical M8 MIDI ports: input=%u output=%u", *input, *output);
  return found;
}

int m8_coremidi_ports_still_exist(void) {
  int source_found = 0, destination_found = 0;
  const ItemCount sources = MIDIGetNumberOfSources();
  const ItemCount destinations = MIDIGetNumberOfDestinations();
  for (ItemCount i = 0; i < sources; i++)
    source_found |= MIDIGetSource(i) == selected_source;
  for (ItemCount i = 0; i < destinations; i++)
    destination_found |= MIDIGetDestination(i) == selected_destination;
  return source_found && destination_found;
}

#endif
