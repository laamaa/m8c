// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT
#ifdef USE_RTMIDI

#ifndef NDEBUG
#define RTMIDI_DEBUG
#endif

#include "../command.h"
#include "../config.h"
#include "m8.h"
#include "queue.h"
#include <SDL3/SDL.h>
#include <rtmidi_c.h>
#include <stdbool.h>
#include <string.h>

RtMidiInPtr midi_in;
RtMidiOutPtr midi_out;
message_queue_s queue;

static uint8_t midi_decode_buffer[2048];

const unsigned char m8_sysex_header[5] = {0xF0, 0x00, 0x02, 0x61, 0x00};
const unsigned int m8_sysex_header_size = sizeof(m8_sysex_header);
const unsigned char sysex_message_end = 0xF7;

bool midi_processing_suspended = false;
bool midi_sysex_received = false;

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

static void midi_callback(double delta_time, const unsigned char *message, size_t message_size,
                          void *user_data) {
  // Unused variables
  (void)delta_time;
  (void)user_data;

  if (midi_processing_suspended || message_size < 5 || !message_is_m8_sysex(message))
    return;

  if (!midi_sysex_received) {
    midi_sysex_received = true;
  }

  size_t decoded_length =
      midi_decode(message, message_size, midi_decode_buffer, sizeof(midi_decode_buffer));

  // If you need to debug incoming MIDI packets, you can uncomment the lines below:

  /* printf("Original data: ");
  for (size_t i = 0; i < message_size; i++) {
    printf("%02X ", message[i]);
  } */

  if (decoded_length > 0) {
    /* printf("\nDecoded MIDI Data: ");
    for (size_t i = 0; i < decoded_length; i++) {
      printf("%02X ", midi_decode_buffer[i]);
    }
    printf("\n"); */
    push_message(&queue, midi_decode_buffer, decoded_length);
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Decoding failed.\n");
  }
}

static void close_and_free_midi_ports(void) {
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Freeing MIDI ports");
  if (midi_in != NULL) {
    rtmidi_in_cancel_callback(midi_in);
    rtmidi_close_port(midi_in);
    rtmidi_in_free(midi_in);
  }
  if (midi_out != NULL) {
    rtmidi_close_port(midi_out);
    rtmidi_out_free(midi_out);
  }
  midi_in = NULL;
  midi_out = NULL;
  midi_sysex_received = false;
}

static int initialize_rtmidi(void) {
  SDL_Log("Initializing rtmidi");
  midi_in = rtmidi_in_create(RTMIDI_API_UNSPECIFIED, "m8c_in", 2048);
  midi_out = rtmidi_out_create(RTMIDI_API_UNSPECIFIED, "m8c_out");
  if (midi_in == NULL || midi_out == NULL) {
    return 0;
  }
  return 1;
}

static int detect_m8_midi_device(const int verbose, const char *preferred_device) {
  if (midi_in->ptr == NULL) {
    midi_in = NULL;
    midi_out = NULL;
    return -1;
  }
  int m8_midi_port_number = -1;
  const unsigned int ports_total_in = rtmidi_get_port_count(midi_in);
  if (verbose)
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Number of MIDI in ports: %d", ports_total_in);
  for (unsigned int port_number = 0; port_number < ports_total_in; port_number++) {
    int port_name_length_in;
    rtmidi_get_port_name(midi_in, port_number, NULL, &port_name_length_in);
    char port_name[port_name_length_in];
    rtmidi_get_port_name(midi_in, port_number, &port_name[0], &port_name_length_in);
    if (verbose)
      SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "MIDI IN port %d, name: %s", port_number, port_name);
    if (SDL_strncmp("M8", port_name, 2) == 0) {
      m8_midi_port_number = port_number;
      if (verbose)
        SDL_Log("Found M8 Input in MIDI port %d", port_number);
      if (preferred_device != NULL && SDL_strcmp(preferred_device, port_name) == 0) {
        SDL_Log("Found preferred device, breaking");
        break;
      }
    }
  }
  return m8_midi_port_number;
}

static int device_still_exists(void) {
  if (midi_in->ptr == NULL || midi_out->ptr == NULL) {
    return 0;
  };

  int m8_midi_port_number = 0;

  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Checking if opened MIDI port still exists");

  m8_midi_port_number = detect_m8_midi_device(0, NULL);
  if (m8_midi_port_number >= 0) {
    return 1;
  }
  return 0;
}

static int disconnect(void) {
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Sending disconnect message to M8");
  const unsigned char disconnect_sysex[8] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'D', 0xF7};
  const int result =
      rtmidi_out_send_message(midi_out, &disconnect_sysex[0], sizeof(disconnect_sysex));
  if (result != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send disconnect");
  }
  close_and_free_midi_ports();
  return !result;
}

int m8_initialize(const int verbose, const char *preferred_device) {
  int m8_midi_port_number = 0;

  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Initialize M8 via RTMIDI called");
  if (midi_in == NULL || midi_out == NULL) {
    initialize_rtmidi();
  };
  m8_midi_port_number = detect_m8_midi_device(verbose, preferred_device);
  if (m8_midi_port_number >= 0) {
    rtmidi_in_ignore_types(midi_in, false, true, true); // Allow sysex
    rtmidi_open_port(midi_in, m8_midi_port_number, "M8");
    rtmidi_open_port(midi_out, m8_midi_port_number, "M8");
    init_queue(&queue);
    return 1;
  }
  return 0;
}

int m8_reset_display(void) {
  SDL_Log("Reset display");
  const unsigned char reset_sysex[8] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'R', 0xF7};
  const int result = rtmidi_out_send_message(midi_out, &reset_sysex[0], sizeof(reset_sysex));
  if (result != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error resetting M8 display, error %s", midi_out->msg);
    return 0;
  }
  return 1;
}

int m8_enable_display(const unsigned char reset_display) {
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Sending enable command sysex");
  rtmidi_in_set_callback(midi_in, midi_callback, NULL);
  const unsigned char disconnect_sysex[8] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'D', 0xF7};
  int result =
      rtmidi_out_send_message(midi_out, &disconnect_sysex[0], sizeof(disconnect_sysex));
  if (result != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send disconnect");
  }
  const unsigned char enable_sysex[8] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'E', 0xF7};
  result = rtmidi_out_send_message(midi_out, &enable_sysex[0], sizeof(enable_sysex));
  if (result != 0) {
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
      close_and_free_midi_ports();
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

  const int result = rtmidi_out_send_message(midi_out, controller_sysex, sizeof(controller_sysex));
  if (result != 0) {
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

    const int result = rtmidi_out_send_message(midi_out, &keyjazz_sysex[0], sizeof(keyjazz_sysex));
    if (result != 0) {
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

  const int result = rtmidi_out_send_message(midi_out, &keyjazz_sysex[0], sizeof(keyjazz_sysex));
  if (result != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send keyjazz input message");
    return 0;
  }
  return 1;
}

int m8_process_data(const config_params_s *conf) {
  static unsigned int empty_cycles = 0;

  if (queue_size(&queue) > 0) {
    unsigned char *command;
    empty_cycles = 0;
    size_t length = 0;
    while ((command = pop_message(&queue, &length)) != NULL) {
      process_command(command, length);
      SDL_free(command);
    }
  } else {
    empty_cycles++;
    if (empty_cycles >= conf->wait_packets) {
      if (device_still_exists()) {
        empty_cycles = 0;
        return DEVICE_PROCESSING;
      }
      SDL_Log("No messages received for %d cycles, assuming device disconnected", empty_cycles);
      close_and_free_midi_ports();
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
  if (midi_in == NULL || midi_out == NULL) {
    initialize_rtmidi();
  };
  const unsigned int ports_total_in = rtmidi_get_port_count(midi_in);
  SDL_Log("Number of MIDI in ports: %d", ports_total_in);
  for (unsigned int port_number = 0; port_number < ports_total_in; port_number++) {
    int port_name_length_in;
    rtmidi_get_port_name(midi_in, port_number, NULL, &port_name_length_in);
    char port_name[port_name_length_in];
    rtmidi_get_port_name(midi_in, port_number, &port_name[0], &port_name_length_in);
    SDL_Log("MIDI IN port %d, name: %s", port_number, port_name);
  }
  close_and_free_midi_ports();
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

#endif
