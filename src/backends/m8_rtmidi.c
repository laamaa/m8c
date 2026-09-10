// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT
#ifdef USE_RTMIDI

#ifndef NDEBUG
#define RTMIDI_DEBUG
#endif

#include "../command.h"
#include "../config.h"
#include "m8.h"
#include "m8_coremidi.h"
#include "queue.h"
#include <SDL3/SDL.h>
#include <rtmidi_c.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>

RtMidiInPtr midi_in;
RtMidiOutPtr midi_out;
message_queue_s queue;

static uint8_t midi_decode_buffer[2048];

const unsigned char m8_sysex_header[5] = {0xF0, 0x00, 0x02, 0x61, 0x00};
const unsigned int m8_sysex_header_size = sizeof(m8_sysex_header);
const unsigned char sysex_message_end = 0xF7;
static const unsigned char disconnect_sysex[8] = {0xF0, 0x00, 0x02, 0x61,
                                                  0x00, 0x00, 'D',  0xF7};

static atomic_bool midi_processing_suspended = false;
static atomic_bool midi_sysex_received = false;
static bool midi_in_open = false;
static bool midi_out_open = false;
static bool midi_callback_set = false;
static bool enable_pending = false;
static Uint64 enable_started_at = 0;
static bool queue_initialized = false;


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

  if (atomic_load(&midi_processing_suspended) || message_size < 5 || !message_is_m8_sysex(message))
    return;

  if (!atomic_load(&midi_sysex_received)) {
    atomic_store(&midi_sysex_received, true);
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
  atomic_store(&midi_processing_suspended, true);
  if (midi_in != NULL) {
    // Stop the producer before releasing the C wrapper's callback userdata.
    if (midi_in_open)
      rtmidi_close_port(midi_in);
    if (midi_callback_set)
      rtmidi_in_cancel_callback(midi_in);
    rtmidi_in_free(midi_in);
  }
  midi_callback_set = false;
  if (midi_out != NULL) {
    if (midi_out_open)
      rtmidi_close_port(midi_out);
    rtmidi_out_free(midi_out);
  }
  midi_in = NULL;
  midi_out = NULL;
  midi_in_open = false;
  midi_out_open = false;
  enable_pending = false;
  if (queue_initialized) {
    destroy_queue(&queue);
    queue_initialized = false;
  }
  atomic_store(&midi_sysex_received, false);
}

static int initialize_rtmidi(void) {
  SDL_Log("Initializing rtmidi");
  midi_in = rtmidi_in_create(RTMIDI_API_UNSPECIFIED, "m8c_in", 2048);
  midi_out = rtmidi_out_create(RTMIDI_API_UNSPECIFIED, "m8c_out");
  if (midi_in == NULL || midi_out == NULL || !midi_in->ptr || !midi_out->ptr ||
      !midi_in->ok || !midi_out->ok) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Cannot create MIDI ports: %s / %s",
                 midi_in ? midi_in->msg : "allocation failed",
                 midi_out ? midi_out->msg : "allocation failed");
    close_and_free_midi_ports();
    return 0;
  }
  return 1;
}

static int get_port_name(RtMidiPtr device, unsigned int port_number, char *name, size_t name_size) {
  int length = (int)name_size;
  const int written = rtmidi_get_port_name(device, port_number, name, &length);
  return device->ok && written >= 0 && (size_t)written < name_size;
}

#ifndef M8C_COREMIDI_DISCOVERY
static unsigned int next_pair;
static unsigned int selected_input, selected_output;
static char selected_name[512];
#endif

static int detect_m8_midi_device(const int verbose, const char *preferred_device,
                                 unsigned int *input_port, unsigned int *output_port) {
#ifdef M8C_COREMIDI_DISCOVERY
  return m8_coremidi_find_ports(verbose, preferred_device, input_port, output_port);
#else
  const unsigned int inputs = rtmidi_get_port_count(midi_in);
  const unsigned int outputs = rtmidi_get_port_count(midi_out);
  if (!midi_in->ok || !midi_out->ok)
    return 0;

  // Prefer an exact requested name, then try the other M8 pairs. Rotate equal-name
  // pairs between attempts so an unresponsive virtual port cannot monopolize retries.
  for (int preferred_only = preferred_device != NULL; preferred_only >= 0; preferred_only--) {
    unsigned int pairs = 0;
    unsigned int first_input = 0, first_output = 0;
    char first_name[512] = "";
    for (unsigned int in = 0; in < inputs; in++) {
      char input_name[512];
      if (!get_port_name(midi_in, in, input_name, sizeof(input_name)))
        continue;
      if (verbose)
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "MIDI IN port %u, name: %s", in, input_name);
      if (SDL_strncmp(input_name, "M8", 2) != 0 ||
          (preferred_only && SDL_strcmp(preferred_device, input_name) != 0))
        continue;
      for (unsigned int out = 0; out < outputs; out++) {
        char output_name[512];
        if (!get_port_name(midi_out, out, output_name, sizeof(output_name)) ||
            SDL_strcmp(input_name, output_name) != 0)
          continue;
        if (pairs++ == 0) {
          first_input = in;
          first_output = out;
          SDL_strlcpy(first_name, input_name, sizeof(first_name));
        }
        if (pairs - 1 == next_pair) {
          *input_port = in;
          *output_port = out;
          SDL_strlcpy(selected_name, input_name, sizeof(selected_name));
          next_pair = pairs;
          return 1;
        }
      }
    }
    if (pairs > 0) {
      *input_port = first_input;
      *output_port = first_output;
      SDL_strlcpy(selected_name, first_name, sizeof(selected_name));
      next_pair = 1;
      return 1;
    }
  }
  return 0;
#endif
}

static int device_still_exists(void) {
  if (midi_in == NULL || midi_out == NULL || !midi_in_open || !midi_out_open) {
    return 0;
  };

#ifdef M8C_COREMIDI_DISCOVERY
  return m8_coremidi_ports_still_exist();
#else
  char input_name[512], output_name[512];
  const unsigned int inputs = rtmidi_get_port_count(midi_in);
  const unsigned int outputs = rtmidi_get_port_count(midi_out);
  return midi_in->ok && midi_out->ok && selected_input < inputs && selected_output < outputs &&
         get_port_name(midi_in, selected_input, input_name, sizeof(input_name)) &&
         get_port_name(midi_out, selected_output, output_name, sizeof(output_name)) &&
         SDL_strcmp(input_name, selected_name) == 0 && SDL_strcmp(output_name, selected_name) == 0;
#endif
}

static int disconnect(void) {
  if (midi_out == NULL || !midi_out_open) {
    close_and_free_midi_ports();
    return 1;
  }
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Sending disconnect message to M8");
  const int result =
      rtmidi_out_send_message(midi_out, &disconnect_sysex[0], sizeof(disconnect_sysex));
  if (result != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send disconnect");
    close_and_free_midi_ports();
    return 0;
  }
  close_and_free_midi_ports();
  return !result;
}

int m8_initialize(const int verbose, const char *preferred_device) {
  unsigned int input_port = 0, output_port = 0;

  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Initialize M8 via RTMIDI called");
  if ((midi_in == NULL || midi_out == NULL) && !initialize_rtmidi())
    return 0;
  if (detect_m8_midi_device(verbose, preferred_device, &input_port, &output_port)) {
    rtmidi_in_ignore_types(midi_in, false, true, true); // Allow sysex
    rtmidi_open_port(midi_in, input_port, "M8");
    if (!midi_in->ok) {
      SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to open M8 MIDI input: %s", midi_in->msg);
      close_and_free_midi_ports();
      return 0;
    }
    midi_in_open = true;
    rtmidi_open_port(midi_out, output_port, "M8");
    if (!midi_out->ok) {
      SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to open M8 MIDI output: %s", midi_out->msg);
      close_and_free_midi_ports();
      return 0;
    }
    midi_out_open = true;
    init_queue(&queue);
    queue_initialized = true;
    atomic_store(&midi_processing_suspended, false);
#ifndef M8C_COREMIDI_DISCOVERY
    selected_input = input_port;
    selected_output = output_port;
#endif
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
  if (midi_in == NULL || midi_out == NULL || !midi_in_open || !midi_out_open)
    return 0;
  if (enable_pending) {
    if (atomic_load(&midi_sysex_received)) {
      enable_pending = false;
      if (reset_display && !m8_reset_display()) {
        close_and_free_midi_ports();
        return 0;
      }
      return 1;
    }
    if (SDL_GetTicks() - enable_started_at > 1000) {
      SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM,
                      "M8 MIDI port did not respond. Connect M8 hardware with firmware 6.0.0 or newer.");
      close_and_free_midi_ports();
      return 0;
    }
    return DEVICE_CONNECTING;
  }
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Sending enable command sysex");
  atomic_store(&midi_sysex_received, false);
  rtmidi_in_set_callback(midi_in, midi_callback, NULL);
  if (!midi_in->ok) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to install MIDI callback: %s", midi_in->msg);
    close_and_free_midi_ports();
    return 0;
  }
  midi_callback_set = true;
  int result =
      rtmidi_out_send_message(midi_out, &disconnect_sysex[0], sizeof(disconnect_sysex));
  if (result != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send disconnect");
    close_and_free_midi_ports();
    return 0;
  }
  const unsigned char enable_sysex[8] = {0xF0, 0x00, 0x02, 0x61, 0x00, 0x00, 'E', 0xF7};
  result = rtmidi_out_send_message(midi_out, &enable_sysex[0], sizeof(enable_sysex));
  if (result != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to send remote display enable command");
    close_and_free_midi_ports();
    return 0;
  }
  enable_started_at = SDL_GetTicks();
  enable_pending = true;
  return DEVICE_CONNECTING;
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
      close_and_free_midi_ports();
      empty_cycles = 0;
      return DEVICE_DISCONNECTED;
    }
  }
  return DEVICE_PROCESSING;
}

int m8_close(void) {
  return disconnect();
}

int m8_list_devices(void) {
  if ((midi_in == NULL || midi_out == NULL) && !initialize_rtmidi())
    return 1;
  const unsigned int ports = rtmidi_get_port_count(midi_in);
  if (!midi_in->ok) {
    close_and_free_midi_ports();
    return 1;
  }
  SDL_Log("Number of MIDI in ports: %u", ports);
  for (unsigned int port = 0; port < ports; port++) {
    char name[512];
    if (get_port_name(midi_in, port, name, sizeof(name)))
      SDL_Log("MIDI IN port %u, name: %s", port, name);
  }
  close_and_free_midi_ports();
  return 0;
}

int m8_pause_processing(void) {
  atomic_store(&midi_processing_suspended, true);
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Pausing MIDI processing");
  return 1;
}
int m8_resume_processing(void) {
  atomic_store(&midi_processing_suspended, false);
  SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Resuming MIDI processing");
  m8_reset_display();
  return 1;
}

#endif
