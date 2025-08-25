#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_error.h"
#ifdef USE_LIBUSB

#include "m8.h"
#include "ringbuffer.h"
#include <SDL3/SDL.h>
#include <errno.h>
#include <libusb.h>

#define EP_ISO_IN 0x85
#define IFACE_NUM 4

#define NUM_TRANSFERS 64
#define PACKET_SIZE 180
#define NUM_PACKETS 2

extern libusb_device_handle *devh;

SDL_AudioStream *sdl_audio_stream = NULL;
int audio_initialized = 0;
RingBuffer *audio_buffer = NULL;
static uint8_t *audio_callback_buffer = NULL;
static size_t audio_callback_buffer_size = 0;
static int audio_prebuffer_filled = 0;
#define PREBUFFER_SIZE (8 * 1024)  // Wait for 8KB before starting playback

static void audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
  (void)userdata;  // Suppress unused parameter warning
  (void)additional_amount;  // Suppress unused parameter warning
  
  // Reallocate callback buffer if needed
  if (audio_callback_buffer_size < (size_t)total_amount) {
    audio_callback_buffer = SDL_realloc(audio_callback_buffer, total_amount);
    if (audio_callback_buffer == NULL) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate audio buffer");
      return;
    }
    audio_callback_buffer_size = (size_t)total_amount;
  }

  // Try to get audio data from ring buffer
  uint32_t available_bytes = audio_buffer->size;
  
  // Check if we have enough data for initial buffering
  if (!audio_prebuffer_filled && available_bytes < PREBUFFER_SIZE) {
    // Not enough data yet, output silence and wait
    SDL_memset(audio_callback_buffer, 0, total_amount);
    if(!SDL_PutAudioStreamData(stream, audio_callback_buffer, total_amount)) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to put audio stream data: %s", SDL_GetError());
    }
    return;
  }
  
  // Mark prebuffer as filled once we have enough data
  if (!audio_prebuffer_filled) {
    audio_prebuffer_filled = 1;
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Audio prebuffer filled, starting playback");
  }

  if (available_bytes >= (uint32_t)total_amount) {
    // We have enough data, read it
    uint32_t read_len = ring_buffer_pop(audio_buffer, audio_callback_buffer, total_amount);
    if (read_len > 0) {
      if(!SDL_PutAudioStreamData(stream, audio_callback_buffer, read_len)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to put audio stream data: %s", SDL_GetError());
      }
    }
  } else if (available_bytes > 0) {
    // We have some data but not enough - read what we can and pad with silence
    uint32_t read_len = ring_buffer_pop(audio_buffer, audio_callback_buffer, available_bytes);
    SDL_memset(audio_callback_buffer + read_len, 0, total_amount - read_len);
    if(!SDL_PutAudioStreamData(stream, audio_callback_buffer, total_amount)) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to put audio stream data: %s", SDL_GetError());
    }
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Partial buffer: %d/%d bytes", available_bytes, total_amount);
  } else {
    // No data available - put silence and reset prebuffer flag
    SDL_memset(audio_callback_buffer, 0, total_amount);
    if(!SDL_PutAudioStreamData(stream, audio_callback_buffer, total_amount)) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to put audio stream data: %s", SDL_GetError());
    }
    audio_prebuffer_filled = 0;  // Reset prebuffer to avoid continuous dropouts
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Buffer underflow! Resetting prebuffer");
  }
}

static void cb_xfr(struct libusb_transfer *xfr) {
  unsigned int i;
  static int error_count = 0;

  for (i = 0; i < (unsigned int)xfr->num_iso_packets; i++) {
    struct libusb_iso_packet_descriptor *pack = &xfr->iso_packet_desc[i];

    if (pack->status != LIBUSB_TRANSFER_COMPLETED) {
      error_count++;
      if (error_count % 100 == 1) { // Log only every 100th error to avoid spam
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "XFR callback error (status %d: %s)", pack->status,
                     libusb_error_name(pack->status));
      }
      continue; // Skip this packet but continue processing others
    }

    if (pack->actual_length > 0) {
      const uint8_t *data = libusb_get_iso_packet_buffer_simple(xfr, i);
      if (sdl_audio_stream != 0 && audio_buffer != NULL) {
        uint32_t actual = ring_buffer_push(audio_buffer, data, pack->actual_length);
        if (actual == (uint32_t)-1) {
          SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Buffer overflow!");
        }
      }
    }
  }

  // Reset error count on successful transfer
  if (xfr->status == LIBUSB_TRANSFER_COMPLETED) {
    error_count = 0;
  }

  int submit_result = libusb_submit_transfer(xfr);
  if (submit_result < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "error re-submitting URB: %s", libusb_error_name(submit_result));
    SDL_free(xfr->buffer);
  }
}

static struct libusb_transfer *xfr[NUM_TRANSFERS];

static int benchmark_in() {
  int i;

  for (i = 0; i < NUM_TRANSFERS; i++) {
    xfr[i] = libusb_alloc_transfer(NUM_PACKETS);
    if (!xfr[i]) {
      SDL_Log("Could not allocate transfer");
      return -ENOMEM;
    }

    Uint8 *buffer = SDL_malloc(PACKET_SIZE * NUM_PACKETS);

    libusb_fill_iso_transfer(xfr[i], devh, EP_ISO_IN, buffer, PACKET_SIZE * NUM_PACKETS,
                             NUM_PACKETS, cb_xfr, NULL, 0);
    libusb_set_iso_packet_lengths(xfr[i], PACKET_SIZE);

    libusb_submit_transfer(xfr[i]);
  }

  return 1;
}

int audio_initialize(const char *output_device_name, unsigned int audio_buffer_size) {
  (void)audio_buffer_size;  // Suppress unused parameter warning
  
  SDL_Log("USB audio setup");

  if (devh == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Device handle is NULL - cannot initialize audio");
    return -1;
  }

  int rc;

  rc = libusb_kernel_driver_active(devh, IFACE_NUM);
  if (rc < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error checking kernel driver status: %s", libusb_error_name(rc));
    return rc;
  }
  if (rc == 1) {
    SDL_Log("Detaching kernel driver");
    rc = libusb_detach_kernel_driver(devh, IFACE_NUM);
    if (rc < 0) {
      SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Could not detach kernel driver: %s", libusb_error_name(rc));
      return rc;
    }
  }

  rc = libusb_claim_interface(devh, IFACE_NUM);
  if (rc < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error claiming interface: %s\n", libusb_error_name(rc));
    return rc;
  }

  rc = libusb_set_interface_alt_setting(devh, IFACE_NUM, 1);
  if (rc < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error setting alt setting: %s\n", libusb_error_name(rc));
    return rc;
  }

  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
      SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Init audio failed %s", SDL_GetError());
      return -1;
    }
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Audio was already initialised");
  }

  static SDL_AudioSpec audio_spec;
  audio_spec.format = SDL_AUDIO_S16;
  audio_spec.channels = 2;
  audio_spec.freq = 44100;

  SDL_Log("Current audio driver is %s and device %s", SDL_GetCurrentAudioDriver(),
          output_device_name);

  // Create larger ring buffer for stable audio - about 1.5 seconds at 44.1kHz stereo 16-bit
  audio_buffer = ring_buffer_create(256 * 1024);

  if (SDL_strcasecmp(SDL_GetCurrentAudioDriver(), "openslES") == 0 || output_device_name == NULL) {
    SDL_Log("Using default audio device");
    sdl_audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec, &audio_callback, &audio_buffer);
  } else {
    // TODO: Implement audio device selection
    sdl_audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec, &audio_callback, &audio_buffer);
  }

  if (sdl_audio_stream == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to open audio stream: %s", SDL_GetError());
    ring_buffer_free(audio_buffer);
    return -1;
  }

  SDL_ResumeAudioStreamDevice(sdl_audio_stream);

  // Good to go
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Starting capture");
  if ((rc = benchmark_in()) < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Capture failed to start: %d", rc);
    return rc;
  }

  audio_initialized = 1;
  audio_prebuffer_filled = 0;  // Reset prebuffer state
  SDL_Log("Successful init");
  return 1;

}

void audio_close() {
  if (devh == NULL) {
    SDL_LogDebug(SDL_LOG_CATEGORY_AUDIO, "Device handle is NULL - audio already closed or not initialized");
    return;
  }
  if (!audio_initialized) {
    SDL_LogDebug(SDL_LOG_CATEGORY_AUDIO, "Audio not initialized - nothing to close");
    return;
  }

  SDL_LogDebug(SDL_LOG_CATEGORY_AUDIO, "Closing audio");

  int rc;

  for (int i = 0; i < NUM_TRANSFERS; i++) {
    rc = libusb_cancel_transfer(xfr[i]);
    if (rc < 0) {
      SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error cancelling transfer: %s\n",
                   libusb_error_name(rc));
    }
  }

  SDL_Log("Freeing interface %d", IFACE_NUM);

  rc = libusb_release_interface(devh, IFACE_NUM);
  if (rc < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error releasing interface: %s\n", libusb_error_name(rc));
    return;
  }

  if (sdl_audio_stream != NULL) {
    SDL_Log("Closing audio device");
    SDL_DestroyAudioStream(sdl_audio_stream);
    sdl_audio_stream = 0;
  }

  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Audio closed");

  ring_buffer_free(audio_buffer);

  // Free callback buffer
  if (audio_callback_buffer) {
    SDL_free(audio_callback_buffer);
    audio_callback_buffer = NULL;
    audio_callback_buffer_size = 0;
  }

  audio_initialized = 0;
  audio_prebuffer_filled = 0;
}

void audio_toggle(const char *output_device_name, unsigned int audio_buffer_size) {
  (void)output_device_name;  // Suppress unused parameter warning
  (void)audio_buffer_size;  // Suppress unused parameter warning
  SDL_Log("Libusb audio toggling not implemented yet");
}

#endif
