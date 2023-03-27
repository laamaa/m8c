#ifdef USE_LIBUSB

#include <libusb.h>
#include <errno.h>
#include <SDL.h>
#include "ringbuffer.h"
#include "usb.h"

#define EP_ISO_IN 0x85
#define IFACE_NUM 4

#define NUM_TRANSFERS 64
#define PACKET_SIZE 180
#define NUM_PACKETS 2

SDL_AudioDeviceID sdl_audio_device_id = 0;
RingBuffer *audio_buffer = NULL;

static void audio_callback(void *userdata, Uint8 *stream,
                           int len) {
  uint32_t read_len = ring_buffer_pop(audio_buffer, stream, len);

  if (read_len == -1) {
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Buffer underflow!");
  }

  // If we didn't read the full len bytes, fill the rest with zeros
  if (read_len < len) {
    SDL_memset(&stream[read_len], 0, len - read_len);
  }
}

static void cb_xfr(struct libusb_transfer *xfr) {
  unsigned int i;

  for (i = 0; i < xfr->num_iso_packets; i++) {
    struct libusb_iso_packet_descriptor *pack = &xfr->iso_packet_desc[i];

    if (pack->status != LIBUSB_TRANSFER_COMPLETED) {
      SDL_Log("XFR callback error (status %d: %s)", pack->status,
              libusb_error_name(pack->status));
      /* This doesn't happen, so bail out if it does. */
      return;
    }

    const uint8_t *data = libusb_get_iso_packet_buffer_simple(xfr, i);
    if (sdl_audio_device_id != 0) {
      uint32_t actual = ring_buffer_push(audio_buffer, data, pack->actual_length);
      if (actual == -1) {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Buffer overflow!");
      }
    }
  }

  if (libusb_submit_transfer(xfr) < 0) {
    SDL_Log("error re-submitting URB\n");
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

    libusb_fill_iso_transfer(xfr[i], devh, EP_ISO_IN, buffer,
                             PACKET_SIZE * NUM_PACKETS, NUM_PACKETS, cb_xfr, NULL, 0);
    libusb_set_iso_packet_lengths(xfr[i], PACKET_SIZE);

    libusb_submit_transfer(xfr[i]);
  }

  return 1;
}

int audio_init(int audio_buffer_size, const char *output_device_name) {
  SDL_Log("USB audio setup");

  int rc;

  rc = libusb_kernel_driver_active(devh, IFACE_NUM);
  if (rc == 1) {
    SDL_Log("Detaching kernel driver");
    rc = libusb_detach_kernel_driver(devh, IFACE_NUM);
    if (rc < 0) {
      SDL_Log("Could not detach kernel driver: %s\n",
              libusb_error_name(rc));
      return rc;
    }
  }

  rc = libusb_claim_interface(devh, IFACE_NUM);
  if (rc < 0) {
    SDL_Log("Error claiming interface: %s\n", libusb_error_name(rc));
    return rc;
  }

  rc = libusb_set_interface_alt_setting(devh, IFACE_NUM, 1);
  if (rc < 0) {
    SDL_Log("Error setting alt setting: %s\n", libusb_error_name(rc));
    return rc;
  }

  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
      SDL_Log("Init audio failed %s", SDL_GetError());
      return -1;
    }
  } else {
    SDL_Log("Audio was already initialised");
  }

  static SDL_AudioSpec audio_spec;
  audio_spec.format = AUDIO_S16;
  audio_spec.channels = 2;
  audio_spec.freq = 44100;
  audio_spec.samples = audio_buffer_size;
  audio_spec.callback = audio_callback;

  SDL_AudioSpec _obtained;
  SDL_zero(_obtained);

  SDL_Log("Current audio driver is %s and device %s", SDL_GetCurrentAudioDriver(),
          output_device_name);

  if (SDL_strcasecmp(SDL_GetCurrentAudioDriver(), "openslES") == 0 || output_device_name == NULL) {
    SDL_Log("Using default audio device");
    sdl_audio_device_id = SDL_OpenAudioDevice(NULL, 0, &audio_spec, &_obtained, 0);
  } else {
    sdl_audio_device_id = SDL_OpenAudioDevice(output_device_name, 0, &audio_spec, &_obtained, 0);
  }

  audio_buffer = ring_buffer_create(4 * _obtained.size);

  SDL_Log("Obtained audio spec. Sample rate: %d, channels: %d, samples: %d, size: %d",
          _obtained.freq,
          _obtained.channels,
          _obtained.samples, +_obtained.size);

  SDL_PauseAudioDevice(sdl_audio_device_id, 0);

  // Good to go
  SDL_Log("Starting capture");
  if ((rc = benchmark_in()) < 0) {
    SDL_Log("Capture failed to start: %d", rc);
    return rc;
  }

  SDL_Log("Successful init");
  return 1;
}

int audio_destroy() {
  SDL_Log("Closing audio");

  int i, rc;

  for (i = 0; i < NUM_TRANSFERS; i++) {
    rc = libusb_cancel_transfer(xfr[i]);
    if (rc < 0) {
      SDL_Log("Error cancelling transfer: %s\n", libusb_error_name(rc));
    }
    SDL_free(xfr[i]->buffer);
  }

  SDL_Log("Freeing interface %d", IFACE_NUM);

  rc = libusb_release_interface(devh, IFACE_NUM);
  if (rc < 0) {
    SDL_Log("Error releasing interface: %s\n", libusb_error_name(rc));
    return rc;
  }

  if (sdl_audio_device_id != 0) {
    SDL_Log("Closing audio device %d", sdl_audio_device_id);
    SDL_AudioDeviceID device = sdl_audio_device_id;
    sdl_audio_device_id = 0;
    SDL_CloseAudioDevice(device);
  }

  SDL_Log("Audio closed");

  ring_buffer_free(audio_buffer);
  return 1;
}

#endif
