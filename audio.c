// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include "audio.h"
#include "SDL_audio.h"
#include <SDL.h>
#include <stdint.h>

static SDL_AudioDeviceID devid_in = 0;
static SDL_AudioDeviceID devid_out = 0;

void audio_cb_in(void *userdata, uint8_t *stream, int len) {
  SDL_QueueAudio(devid_out, stream, len);
}

int audio_init(int audio_buffer_size) {

  int i = 0;
  int m8_found = 0;
  int devcount_in = 0; // audio input device count

  devcount_in = SDL_GetNumAudioDevices(SDL_TRUE);

  if (devcount_in < 1) {
    SDL_Log("No audio capture devices, SDL Error: %s", SDL_GetError());
    return 0;
  } else {
    for (i = 0; i < devcount_in; i++) {
      // Check if input device exists before doing anything else
      if (strcmp(SDL_GetAudioDeviceName(i, SDL_TRUE), "M8 Analog Stereo") ==
          0) {
        SDL_Log("M8 Audio Input device found");
        m8_found = 1;
      }
    }
    if (m8_found == 0) {
      // forget about it
      return 0;
    }

    SDL_AudioSpec want_in, have_in, want_out, have_out;

    // Open output device first to avoid possible Directsound errors
    SDL_zero(want_out);
    want_out.freq = 44100;
    want_out.format = AUDIO_S16;
    want_out.channels = 2;
    want_out.samples = audio_buffer_size;
    devid_out = SDL_OpenAudioDevice(NULL, 0, &want_out, &have_out,
                                    SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (devid_out == 0) {
      SDL_Log("Failed to open output: %s", SDL_GetError());
      return 0;
    }
    SDL_Log("Using default audio output device: %s",SDL_GetAudioDeviceName(0, 0));

    SDL_zero(want_in);
    want_in.freq = 44100;
    want_in.format = AUDIO_S16;
    want_in.channels = 2;
    want_in.samples = audio_buffer_size;
    want_in.callback = audio_cb_in;
    devid_in = SDL_OpenAudioDevice("M8 Analog Stereo", SDL_TRUE, &want_in,
                                   &have_in, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (devid_in == 0) {
      SDL_Log("Failed to open M8 audio device, SDL Error: %s", SDL_GetError());
      return 0;
    }
  }

  // Start audio processing
  SDL_Log("Opening audio devices");
  SDL_PauseAudioDevice(devid_in, 0);
  SDL_PauseAudioDevice(devid_out, 0);

  return 1;
}

void audio_destroy() {
  SDL_Log("Closing audio devices");
  SDL_PauseAudioDevice(devid_in, 1);
  SDL_PauseAudioDevice(devid_out, 1);
  SDL_CloseAudioDevice(devid_in);
  SDL_CloseAudioDevice(devid_out);
}
