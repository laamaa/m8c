// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT
#ifndef USE_LIBUSB
#include "audio.h"
#include <SDL3/SDL.h>

SDL_AudioStream *audio_stream_in, *audio_stream_out;

static unsigned int audio_paused = 0;
static unsigned int audio_initialized = 0;
static SDL_AudioSpec audio_spec_in = {SDL_AUDIO_S16LE, 2, 44100};

static void SDLCALL audio_cb_out(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
  // suppress compiler warnings
  (void)userdata;

  if (additional_amount <= 0) {
    return;
  }

  const int bytes_available = SDL_GetAudioStreamAvailable(audio_stream_in);
  if (bytes_available == -1) {
    SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                 "Error getting available audio stream bytes: %s, destroying audio",
                 SDL_GetError());
    audio_close();
    return;
  }

  // Decide how much to feed this time.
  int to_write_goal = additional_amount;
  if (total_amount > 0) {
    const int prefill_cap = additional_amount * 2;
    if (to_write_goal < total_amount) {
      to_write_goal = SDL_min(total_amount, prefill_cap);
    }
  }

  int to_write = to_write_goal;
  Uint8 temp[4096];

  while (to_write > 0) {
    int still_avail = SDL_GetAudioStreamAvailable(audio_stream_in);
    if (still_avail <= 0) {
      break; // nothing more to pull now
    }

    int chunk = still_avail;
    if (chunk > (int)sizeof(temp)) chunk = (int)sizeof(temp);
    if (chunk > to_write) chunk = to_write;

    const int got = SDL_GetAudioStreamData(audio_stream_in, temp, chunk);
    if (got == -1) {
      SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                   "Error reading audio stream data: %s, destroying audio",
                   SDL_GetError());
      audio_close();
      return;
    }
    if (got == 0) {
      break; // no data currently available
    }

    if (!SDL_PutAudioStreamData(stream, temp, got)) {
      SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                   "Error putting audio stream data: %s, destroying audio",
                   SDL_GetError());
      audio_close();
      return;
    }

    to_write -= got;
  }
}

void audio_toggle(const char *output_device_name, unsigned int audio_buffer_size) {
  if (!audio_initialized) {
    audio_initialize(output_device_name, audio_buffer_size);
    return;
  }
  if (audio_paused) {
    SDL_ResumeAudioStreamDevice(audio_stream_out);
    SDL_ResumeAudioStreamDevice(audio_stream_in);
  } else {
    SDL_PauseAudioStreamDevice(audio_stream_in);
    SDL_PauseAudioStreamDevice(audio_stream_out);
  }
  audio_paused = !audio_paused;
  SDL_Log(audio_paused ? "Audio paused" : "Audio resumed");
}

int audio_initialize(const char *output_device_name, const unsigned int audio_buffer_size) {

  int num_devices_in, num_devices_out;
  SDL_AudioDeviceID m8_device_id = 0;
  SDL_AudioDeviceID output_device_id = 0;

  if (SDL_Init(SDL_INIT_AUDIO) == false) {
    SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "SDL Audio init failed, SDL Error: %s", SDL_GetError());
    return 0;
  }

  SDL_AudioDeviceID *devices_in = SDL_GetAudioRecordingDevices(&num_devices_in);
  if (!devices_in) {
    SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "No audio capture devices, SDL Error: %s", SDL_GetError());
    return 0;
  }

  SDL_AudioDeviceID *devices_out = SDL_GetAudioPlaybackDevices(&num_devices_out);
  if (!devices_out) {
    SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "No audio playback devices, SDL Error: %s",
                 SDL_GetError());
    return 0;
  }

  SDL_LogDebug(SDL_LOG_CATEGORY_AUDIO, "Audio input devices:");
  for (int i = 0; i < num_devices_in; i++) {
    const SDL_AudioDeviceID instance_id = devices_in[i];
    const char *device_name = SDL_GetAudioDeviceName(instance_id);
    SDL_LogDebug(SDL_LOG_CATEGORY_AUDIO, "%s", device_name);
    if (SDL_strstr(device_name, "M8") != NULL) {
      SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, "M8 Audio Input device found: %s", device_name);
      m8_device_id = instance_id;
    }
  }

  if (output_device_name != NULL) {
    for (int i = 0; i < num_devices_out; i++) {
      const SDL_AudioDeviceID instance_id = devices_out[i];
      const char *device_name = SDL_GetAudioDeviceName(instance_id);
      SDL_LogDebug(SDL_LOG_CATEGORY_AUDIO, "%s", device_name);
      if (SDL_strcasestr(device_name, output_device_name) != NULL) {
        SDL_Log("Requested output device found: %s", device_name);
        output_device_id = instance_id;
      }
    }
  }

  SDL_free(devices_in);
  SDL_free(devices_out);

  if (!output_device_id) {
    output_device_id = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
  }

  if (!m8_device_id) {
    // forget about it
    SDL_Log("Cannot find M8 audio input device");
    return 0;
  }

  char audio_buffer_size_str[256];
  SDL_snprintf(audio_buffer_size_str, sizeof(audio_buffer_size_str), "%d", audio_buffer_size);
  if (audio_buffer_size > 0) {
    SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, "Setting requested audio device sample frames to %d",
                audio_buffer_size);
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, audio_buffer_size_str);
  }

  audio_stream_out = SDL_OpenAudioDeviceStream(output_device_id, NULL, audio_cb_out, NULL);

  SDL_AudioSpec audio_spec_out;
  int audio_out_buffer_size_real, audio_in_buffer_size_real = 0;

  SDL_GetAudioDeviceFormat(output_device_id, &audio_spec_out, &audio_out_buffer_size_real);

  if (!audio_stream_out) {
    SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Error opening audio output device: %s", SDL_GetError());
    return 0;
  }
  SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO,
              "Opening audio output: rate %dhz, buffer size: %d frames", audio_spec_out.freq,
              audio_out_buffer_size_real);

  audio_stream_in = SDL_OpenAudioDeviceStream(m8_device_id, &audio_spec_in, NULL, NULL);
  if (!audio_stream_in) {
    SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Error opening audio input device: %s", SDL_GetError());
    SDL_DestroyAudioStream(audio_stream_out);
    return 0;
  }

  SDL_SetAudioStreamFormat(audio_stream_in, &audio_spec_in, &audio_spec_out);
  SDL_GetAudioDeviceFormat(m8_device_id, &audio_spec_in, &audio_in_buffer_size_real);
  SDL_LogDebug(SDL_LOG_CATEGORY_AUDIO, "Audiospec In: format %d, channels %d, rate %d, buffer size %d frames",
               audio_spec_in.format, audio_spec_in.channels, audio_spec_in.freq, audio_in_buffer_size_real);



  SDL_ResumeAudioStreamDevice(audio_stream_out);
  SDL_ResumeAudioStreamDevice(audio_stream_in);

  audio_paused = 0;
  audio_initialized = 1;

  return 1;
}

void audio_close(void) {
  if (!audio_initialized)
    return;
  SDL_Log("Closing audio devices");
  SDL_DestroyAudioStream(audio_stream_in);
  SDL_DestroyAudioStream(audio_stream_out);
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
  audio_initialized = 0;
}

#endif
