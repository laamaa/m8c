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

const unsigned char m8_sysex_header[5] = {0xF0, 0x00, 0x02, 0x61, 0x00};
const unsigned int m8_sysex_header_size = sizeof(m8_sysex_header);
const unsigned char sysex_message_end = 0xF7;

bool message_is_m8_sysex(const unsigned char *message) {
  if (memcmp(m8_sysex_header, message, m8_sysex_header_size) == 0) {
    return true;
  }
  return false;
}

void midi_decode(const uint8_t *encoded_data, size_t length, uint8_t **decoded_data,
                 size_t *decoded_length) {
  if (length < m8_sysex_header_size) {
    // Invalid data
    *decoded_data = NULL;
    *decoded_length = 0;
    return;
  }

  // Skip header "F0 00 02 61" and the first MSB byte
  size_t pos = m8_sysex_header_size + 1;

  // Calculate expected output size (ignoring EOT if present)
  const size_t expected_output_size =
      (length - m8_sysex_header_size) - ((length - m8_sysex_header_size) / 8);
  if (encoded_data[length - 1] == sysex_message_end) {
    length--; // Ignore the EOT byte
  }

  // Allocate memory for decoded output
  *decoded_data = (uint8_t *)SDL_malloc(expected_output_size);
  if (*decoded_data == NULL) {
    *decoded_length = 0;
    return;
  }

  uint8_t bit_counter = 0;
  uint8_t bit_byte_counter = 0;
  uint8_t *out = *decoded_data;
  *decoded_length = 0;

  while (pos < length) {
    // Extract MSB from the "bit field" position
    uint8_t msb = (encoded_data[bit_byte_counter * 8 + m8_sysex_header_size] >> bit_counter) & 0x01;

    // Extract LSB from data byte
    uint8_t lsb = encoded_data[pos] & 0x7F;

    // Reconstruct original byte, skipping the MSB bytes in output
    *out = (msb << 7) | lsb;
    out++;
    (*decoded_length)++;

    bit_counter++;
    pos++;

    if (bit_counter == 7) {
      bit_counter = 0;
      bit_byte_counter++;
      pos++; // Skip the MSB byte
    }
  }
}

static void midi_callback(double delta_time, const unsigned char *message, size_t message_size,
                          void *user_data) {
  // Unused variables
  (void)delta_time;
  (void)user_data;

  if (message_size < 5 || !message_is_m8_sysex(message))
    return;

  unsigned char *decoded_data;
  size_t decoded_length;
  midi_decode(message, message_size, &decoded_data, &decoded_length);

  // If you need to debug incoming MIDI packets, you can uncomment the lines below:

  /* printf("Original data: ");
  for (size_t i = 0; i < message_size; i++) {
    printf("%02X ", message[i]);
  } */

  if (decoded_data) {
    /* printf("\nDecoded MIDI Data: ");
    for (size_t i = 0; i < decoded_length; i++) {
      printf("%02X ", decoded_data[i]);
    }
    printf("\n"); */
    push_message(&queue, decoded_data, decoded_length);
    SDL_free(decoded_data);
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Decoding failed.\n");
  }
}

int initialize_rtmidi() {
  SDL_Log("Initializing rtmidi");
  midi_in = rtmidi_in_create(RTMIDI_API_UNSPECIFIED, "m8c_in", 1024);
  midi_out = rtmidi_out_create(RTMIDI_API_UNSPECIFIED, "m8c_out");
  if (midi_in == NULL || midi_out == NULL) {
    return 0;
  }
  return 1;
}

int detect_m8_midi_device(const int verbose, const char *preferred_device) {
  int m8_midi_port_number = -1;
  const unsigned int ports_total_in = rtmidi_get_port_count(midi_in);
  if (verbose)
    SDL_Log("Number of MIDI in ports: %d", ports_total_in);
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

int m8_enable_and_reset_display(void) {
  rtmidi_in_set_callback(midi_in, midi_callback, NULL);
  const unsigned char enable_sysex[8] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'E', 0xF7};
  int result = rtmidi_out_send_message(midi_out, &enable_sysex[0], sizeof(enable_sysex));
  if (result != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to enable display");
    return 0;
  }
  result = m8_reset_display();
  return result;
}

void close_and_free_midi_ports(void) {
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Freeing MIDI ports");
  rtmidi_close_port(midi_in);
  rtmidi_close_port(midi_out);
  rtmidi_in_free(midi_in);
  rtmidi_out_free(midi_out);
  midi_in = NULL;
  midi_out = NULL;
}

int disconnect(void) {
  const unsigned char disconnect_sysex[8] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'D', 0xF7};
  const int result =
      rtmidi_out_send_message(midi_out, &disconnect_sysex[0], sizeof(disconnect_sysex));
  if (result != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send disconnect");
  }
  close_and_free_midi_ports();
  return !result;
}

int m8_send_msg_controller(const unsigned char input) {
  SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "MIDI key inputs not implemented yet");
  return 0;
#if 0
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Sending controller input 0x%02X", input);
  // Encode a 8bit byte to two 7-bit bytes
  //const unsigned char sysex_encoded_input[2] = {input & 0x7F, (input >> 7) & 0x01};
  const unsigned char input_sysex[10] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'C', sysex_encoded_input[0], sysex_encoded_input[1], 0xF7};
  const int result = rtmidi_out_send_message(midi_out, &input_sysex[0], sizeof(input_sysex));
  if (result != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send key input message");
    return 0;
  }
  return 1;
#endif
}

int m8_send_msg_keyjazz(const unsigned char note, unsigned char velocity) {
  SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "MIDI keyjazz not implemented yet");
  return 0;
#if 0
  if (velocity > 0x7F) {
    velocity = 0x7F;
  }

  // Special case for note off
  if (note == 0xFF && velocity == 0x00) {
    const unsigned char all_notes_off_sysex[9] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'K', 0xFF, 0xF7};
    const int result =
        rtmidi_out_send_message(midi_out, &all_notes_off_sysex[0], sizeof(all_notes_off_sysex));
    if (result != 0) {
      SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send all notes off");
      return 0;
    }
    return 1;
  }

  const unsigned char keyjazz_sysex[10] = {0xF0, 0x00, 0x02, 0x61,     0x00,
                                           0x00, 'K',  note, velocity, 0xF7};
  const int result = rtmidi_out_send_message(midi_out, &keyjazz_sysex[0], sizeof(keyjazz_sysex));
  if (result != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send keyjazz input message");
    return 0;
  }
  return 1;
#endif
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
      SDL_Log("No messages received for %d cycles, assuming device disconnected", empty_cycles);
      close_and_free_midi_ports();
      return 0;
    }
  }
  return 1;
}

int m8_close() {
  if (queue.mutex != NULL) {
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Destroying command queue");
    SDL_DestroyMutex(queue.mutex);
    SDL_DestroyCondition(queue.cond);
  }
  return disconnect();
}

int m8_list_devices() {
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

#endif
