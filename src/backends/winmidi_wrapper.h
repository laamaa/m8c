// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT
// Windows native MIDI backend wrapper header

#ifndef WINMIDI_WRAPPER_H
#define WINMIDI_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

// Callback function type for receiving MIDI messages
typedef void (*winmidi_callback_fn)(const uint8_t *data, size_t length, void *user_data);

// Initialize the Windows MIDI subsystem (initializes WinRT)
// Returns 1 on success, 0 on failure
int winmidi_init(void);

// Cleanup the Windows MIDI subsystem
void winmidi_cleanup(void);

// List available MIDI devices to the log
// Returns the number of devices found
int winmidi_list_devices(void);

// Detect M8 MIDI device
// verbose: if non-zero, log device enumeration
// preferred_device: if non-NULL, prefer this device name
// Returns device index on success, -1 if not found
int winmidi_detect_m8(int verbose, const char *preferred_device);

// Open MIDI ports for the device at the given index
// Returns 1 on success, 0 on failure
int winmidi_open(int device_index);

// Close MIDI ports
void winmidi_close(void);

// Send a SysEx message
// data: pointer to the SysEx data (including F0 and F7)
// length: length of the data
// Returns 1 on success, 0 on failure
int winmidi_send_sysex(const uint8_t *data, size_t length);

// Set callback for receiving MIDI messages
// callback: function to call when MIDI data is received
// user_data: user data to pass to the callback
void winmidi_set_callback(winmidi_callback_fn callback, void *user_data);

// Check if the opened device still exists
// Returns 1 if device exists, 0 if disconnected
int winmidi_device_exists(void);

#ifdef __cplusplus
}
#endif

#endif // WINMIDI_WRAPPER_H
