// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT
// Windows native MIDI backend for m8c

#ifdef USE_WINMIDI

#include "../command.h"
#include "../config.h"
#include "m8.h"
#include "queue.h"
#include "winmidi_wrapper.h"
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <string.h>

static message_queue_s queue;
static uint8_t midi_decode_buffer[2048];

static const unsigned char m8_sysex_header[5] = {0xF0, 0x00, 0x02, 0x61, 0x00};
static const unsigned int m8_sysex_header_size = sizeof(m8_sysex_header);
static const unsigned char sysex_message_end = 0xF7;

static bool midi_processing_suspended = false;
static bool midi_sysex_received = false;
static bool midi_initialized = false;

static bool message_is_m8_sysex(const unsigned char *message) {
  if (memcmp(m8_sysex_header, message, m8_sysex_header_size) == 0) {
    return true;
  }
  return false;
}

static size_t midi_decode(const uint8_t *encoded_data, size_t length, uint8_t *out_buffer,
                          size_t out_buffer_size) {
  if (length < m8_sysex_header_size) {
    return 0;
  }

  // Skip header "F0 00 02 61" and the first MSB byte
  size_t pos = m8_sysex_header_size + 1;

  // Calculate expected output size (ignoring EOT if present)
  const size_t expected_output_size =
      (length - m8_sysex_header_size) - ((length - m8_sysex_header_size) / 8);
  if (encoded_data[length - 1] == sysex_message_end) {
    length--; // Ignore the EOT byte
  }

  // Check if output buffer is large enough
  if (expected_output_size > out_buffer_size) {
    return 0;
  }

  uint8_t bit_counter = 0;
  uint8_t bit_byte_counter = 0;
  uint8_t *out = out_buffer;
  size_t decoded_length = 0;

  while (pos < length) {
    // Extract MSB from the "bit field" position
    uint8_t msb = (encoded_data[bit_byte_counter * 8 + m8_sysex_header_size] >> bit_counter) & 0x01;

    // Extract LSB from data byte
    uint8_t lsb = encoded_data[pos] & 0x7F;

    // Reconstruct original byte, skipping the MSB bytes in output
    *out = (msb << 7) | lsb;
    out++;
    decoded_length++;

    bit_counter++;
    pos++;

    if (bit_counter == 7) {
      bit_counter = 0;
      bit_byte_counter++;
      pos++; // Skip the MSB byte
    }
  }

  return decoded_length;
}

static void midi_callback(const uint8_t *message, size_t message_size, void *user_data) {
  (void)user_data;

  if (midi_processing_suspended || message_size < 5 || !message_is_m8_sysex(message))
    return;

  if (!midi_sysex_received) {
    midi_sysex_received = true;
  }

  size_t decoded_length =
      midi_decode(message, message_size, midi_decode_buffer, sizeof(midi_decode_buffer));

  if (decoded_length > 0) {
    push_message(&queue, midi_decode_buffer, decoded_length);
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Decoding failed.\n");
  }
}

static void close_midi_ports(void) {
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Closing MIDI ports");
  winmidi_close();
  midi_sysex_received = false;
}

static int initialize_winmidi(void) {
  if (midi_initialized) {
    return 1;
  }

  SDL_Log("Initializing Windows MIDI");
  if (!winmidi_init()) {
    return 0;
  }
  midi_initialized = true;
  return 1;
}

static int device_still_exists(void) {
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Checking if opened MIDI port still exists");
  return winmidi_device_exists();
}

static int disconnect(void) {
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Sending disconnect message to M8");
  const unsigned char disconnect_sysex[8] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'D', 0xF7};
  const int result = winmidi_send_sysex(disconnect_sysex, sizeof(disconnect_sysex));
  if (!result) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send disconnect");
  }
  close_midi_ports();
  return result;
}

int m8_initialize(const int verbose, const char *preferred_device) {
  int m8_midi_port_number = 0;

  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Initialize M8 via Windows MIDI called");
  if (!midi_initialized) {
    if (!initialize_winmidi()) {
      return 0;
    }
  }

  m8_midi_port_number = winmidi_detect_m8(verbose, preferred_device);
  if (m8_midi_port_number >= 0) {
    if (!winmidi_open(m8_midi_port_number)) {
      return 0;
    }
    init_queue(&queue);
    return 1;
  }
  return 0;
}

int m8_reset_display(void) {
  SDL_Log("Reset display");
  const unsigned char reset_sysex[8] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'R', 0xF7};
  const int result = winmidi_send_sysex(reset_sysex, sizeof(reset_sysex));
  if (!result) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error resetting M8 display");
    return 0;
  }
  return 1;
}

int m8_enable_display(const unsigned char reset_display) {
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Sending enable command sysex");

  // Set up the callback
  winmidi_set_callback(midi_callback, NULL);

  // Send disconnect first
  const unsigned char disconnect_sysex[8] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'D', 0xF7};
  int result = winmidi_send_sysex(disconnect_sysex, sizeof(disconnect_sysex));
  if (!result) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send disconnect");
  }

  // Send enable command
  const unsigned char enable_sysex[8] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'E', 0xF7};
  result = winmidi_send_sysex(enable_sysex, sizeof(enable_sysex));
  if (!result) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send remote display enable command");
    return 0;
  }

  // Wait a second to get a reply from the device
  const Uint64 timer_wait_for_response = SDL_GetTicks();
  while (!midi_sysex_received) {
    if (SDL_GetTicks() - timer_wait_for_response > 1000) {
      SDL_LogCritical(
          SDL_LOG_CATEGORY_SYSTEM,
          "No response from device. Please make sure you're using M8 firmware 6.0.0 or newer.");
      close_midi_ports();
      return 0;
    }
    SDL_Delay(5);
  }

  if (reset_display) {
    return m8_reset_display();
  }
  return 1;
}

int m8_send_msg_controller(const unsigned char input) {
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Sending controller input 0x%02X", input);

  // Get MSB from input
  const uint8_t msb = 0 | (input & 0x80 ? 1u << 2 : 0);
  const unsigned char controller_sysex[10] = {
      0xF0, 0x00, 0x02, 0x61, 0x00, msb, 0x00, (uint8_t)('C' & 0x7F), (uint8_t)(input & 0x7F),
      0xF7};

  const int result = winmidi_send_sysex(controller_sysex, sizeof(controller_sysex));
  if (!result) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send key input message");
    return 0;
  }
  return 1;
}

int m8_send_msg_keyjazz(const unsigned char note, unsigned char velocity) {
  if (velocity > 0x7F) {
    velocity = 0x7F;
  }

  // Special case for note off
  if (note == 0xFF && velocity == 0x00) {
    const uint8_t msb = 0 | 1u << 2;
    const unsigned char keyjazz_sysex[10] = {
        0xF0, 0x00, 0x02, 0x61, 0x00,
        msb, 0x00, (uint8_t)('K' & 0x7F), (uint8_t)(0xFF & 0x7F),
        0xF7
    };

    const int result = winmidi_send_sysex(keyjazz_sysex, sizeof(keyjazz_sysex));
    if (!result) {
      SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send all notes off");
      return 0;
    }
    return 1;
  }

  // Get MSB from input
  const uint8_t msb = 0 | (note & 0x80 ? 1u << 2 : 0) | (velocity & 0x80 ? 1u << 3 : 0);
  const unsigned char keyjazz_sysex[11] = {
    0xF0, 0x00, 0x02, 0x61, 0x00,
    msb, 0x00, (uint8_t)('K' & 0x7F), (uint8_t)(note & 0x7F), (uint8_t)(velocity & 0x7F),
    0xF7
  };

  const int result = winmidi_send_sysex(keyjazz_sysex, sizeof(keyjazz_sysex));
  if (!result) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send keyjazz input message");
    return 0;
  }
  return 1;
}

int m8_process_data(const config_params_s *conf) {
  static unsigned int empty_cycles = 0;
  static message_batch_s batch;

  if (pop_all_messages(&queue, &batch) > 0) {
    empty_cycles = 0;
    for (unsigned int i = 0; i < batch.count; i++) {
      process_command(batch.messages[i], batch.lengths[i]);
      SDL_free(batch.messages[i]);
    }
  } else {
    empty_cycles++;
    if (empty_cycles >= conf->wait_packets) {
      if (device_still_exists()) {
        empty_cycles = 0;
        return DEVICE_PROCESSING;
      }
      SDL_Log("No messages received for %d cycles, assuming device disconnected", empty_cycles);
      close_midi_ports();
      destroy_queue(&queue);
      empty_cycles = 0;
      return DEVICE_DISCONNECTED;
    }
  }
  return DEVICE_PROCESSING;
}

int m8_close(void) {
  const int result = disconnect();
  destroy_queue(&queue);
  return result;
}

int m8_list_devices(void) {
  if (!midi_initialized) {
    if (!initialize_winmidi()) {
      return 0;
    }
  }
  winmidi_list_devices();
  return 1;
}

int m8_pause_processing(void) {
  midi_processing_suspended = true;
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Pausing MIDI processing");
  return 1;
}

int m8_resume_processing(void) {
  midi_processing_suspended = false;
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Resuming MIDI processing");
  m8_reset_display();
  return 1;
}

#endif // USE_WINMIDI
