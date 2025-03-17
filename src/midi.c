// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT
#ifdef USE_RTMIDI

#include "midi.h"
#include "command.h"
#include "config.h"
#include "queue.h"
#include <SDL3/SDL.h>
#include <rtmidi_c.h>
#include <stdbool.h>
#include <stdio.h>
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

  uint8_t bitCounter = 0;
  uint8_t bitByteCounter = 0;
  uint8_t *out = *decoded_data;
  *decoded_length = 0;

  while (pos < length) {
    // Extract MSB from the "bit field" position
    uint8_t msb = (encoded_data[bitByteCounter * 8 + m8_sysex_header_size] >> bitCounter) & 0x01;

    // Extract LSB from data byte
    uint8_t lsb = encoded_data[pos] & 0x7F;

    // Reconstruct original byte, skipping the MSB bytes in output
    *out = (msb << 7) | lsb;
    out++;
    (*decoded_length)++;

    bitCounter++;
    pos++;

    if (bitCounter == 7) {
      bitCounter = 0;
      bitByteCounter++;
      pos++; // Skip the MSB byte
    }
  }
}

static void midi_callback(double delta_time, const unsigned char *message, size_t message_size,
                          void *user_data) {
  if (message_size < 5 || !message_is_m8_sysex(message))
    return;

  unsigned char *decoded_data;
  size_t decoded_length;
  midi_decode(message, message_size, &decoded_data, &decoded_length);

  // printf("Original data: ");
  for (size_t i = 0; i < message_size; i++) {
    // printf("%02X ", message[i]);
  }

  if (decoded_data) {
    // printf("\nDecoded MIDI Data: ");
    for (size_t i = 0; i < decoded_length; i++) {
      // printf("%02X ", decoded_data[i]);
    }
    // printf("\n");
    push_message(&queue, decoded_data, decoded_length);
    SDL_free(decoded_data);
  } else {
    printf("Decoding failed.\n");
  }
}

int initialize_rtmidi(void) {
  SDL_Log("init rtmidi");
  midi_in = rtmidi_in_create(RTMIDI_API_UNSPECIFIED, "m8c_in", 1024);
  midi_out = rtmidi_out_create(RTMIDI_API_UNSPECIFIED, "m8c_out");
  if (midi_in == NULL || midi_out == NULL) {
    return 0;
  }
  return 1;
}

int init_serial(const int verbose, const char *preferred_device) {

  init_queue(&queue);

  int midi_in_initialized = 0;
  int midi_out_initialized = 0;

  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "init_serial called");
  if (midi_in == NULL || midi_out == NULL) {
    initialize_rtmidi();
  };
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
    if (strncmp("M8", port_name, 2) == 0) {
      if (verbose)
        SDL_Log("Found M8 Input in MIDI port %d", port_number);
      rtmidi_in_ignore_types(midi_in, false, true, true);
      rtmidi_open_port(midi_in, port_number, "M8");
      midi_in_initialized = 1;
      rtmidi_open_port(midi_out, port_number, "M8");
      midi_out_initialized = 1;
    }
  }
  return (midi_in_initialized && midi_out_initialized);
}

int check_serial_port(void) {
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "check_serial_port called");
  return 0;
}

int reset_display(void) {
  SDL_Log("Reset display\n");
  const unsigned char reset_sysex[8] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'R', 0xF7};
  const int result = rtmidi_out_send_message(midi_out, &reset_sysex[0], sizeof(reset_sysex));
  if (result != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error resetting M8 display, code %d", result);
    return 0;
  }
  return 1;
}

int enable_and_reset_display(void) {
  rtmidi_in_set_callback(midi_in, midi_callback, NULL);
  const unsigned char enable_sysex[8] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'E', 0xF7};
  int result = rtmidi_out_send_message(midi_out, &enable_sysex[0], sizeof(enable_sysex));
  if (result != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to enable display");
    return 0;
  }
  result = reset_display();
  return result;
}

int disconnect(void) {
  const unsigned char disconnect_sysex[8] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'D', 0xF7};
  const int result =
      rtmidi_out_send_message(midi_out, &disconnect_sysex[0], sizeof(disconnect_sysex));
  if (result != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send disconnect");
  }
  SDL_Log("Freeing MIDI ports");
  rtmidi_in_free(midi_in);
  rtmidi_out_free(midi_out);
  return !result;
}

int send_msg_controller(const unsigned char input) {
  const unsigned char input_sysex[9] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'C', input, 0xF7};
  const int result = rtmidi_out_send_message(midi_out, &input_sysex[0], sizeof(input_sysex));
  if (result != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send key input message");
    return 0;
  }
  return 1;
}

int send_msg_keyjazz(const unsigned char note, unsigned char velocity) {
  if (velocity > 0x7F) {
    velocity = 0x7F;
  }
  const unsigned char keyjazz_sysex[10] = {0xF0, 0x00, 0x02, 0x61,     0x00,
                                           0x00, 'K',  note, velocity, 0xF7};
  const int result = rtmidi_out_send_message(midi_out, &keyjazz_sysex[0], sizeof(keyjazz_sysex));
  if (result != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send keyjazz input message");
    return 0;
  }
  return 1;
}

int process_serial(config_params_s conf) {
  unsigned char *command;
  size_t length = 0;
  while ((command = pop_message(&queue, &length)) != NULL) {
    process_command(command, length);
  }
  return 1;
}

int destroy_serial() {
  if (queue.mutex != NULL) {
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Destroying command queue");
    SDL_DestroyMutex(queue.mutex);
    SDL_DestroyCondition(queue.cond);
  }
  return disconnect();
}

#endif
