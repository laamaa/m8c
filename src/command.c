// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include <SDL_log.h>

#include "command.h"
#include "render.h"

// Convert 2 little-endian 8bit bytes to a 16bit integer
static uint16_t decodeInt16(uint8_t *data, uint8_t start) {
  return data[start] | (uint16_t)data[start + 1] << 8;
}

enum m8_command_bytes {
  draw_rectangle_command = 0xFE,
  draw_rectangle_command_datalength = 12,
  draw_character_command = 0xFD,
  draw_character_command_datalength = 12,
  draw_oscilloscope_waveform_command = 0xFC,
  draw_oscilloscope_waveform_command_mindatalength = 1 + 3,
  draw_oscilloscope_waveform_command_maxdatalength = 1 + 3 + 320,
  joypad_keypressedstate_command = 0xFB,
  joypad_keypressedstate_command_datalength = 3,
  system_info_command = 0xFF,
  system_info_command_datalength = 6
};

static inline void dump_packet(uint32_t size, uint8_t *recv_buf) {
  for (uint16_t a = 0; a < size; a++) {
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "0x%02X ", recv_buf[a]);
  }
  SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "\n");
}

int process_command(uint8_t *data, uint32_t size) {

  uint8_t recv_buf[size + 1];

  memcpy(recv_buf, data, size);
  recv_buf[size] = 0;

  switch (recv_buf[0]) {

  case draw_rectangle_command:

    if (size != draw_rectangle_command_datalength) {
      SDL_LogError(
          SDL_LOG_CATEGORY_ERROR,
          "Invalid draw rectangle packet: expected length %d, got %d\n",
          draw_rectangle_command_datalength, size);
      dump_packet(size, recv_buf);
      return 0;
      break;
    } else {

      struct draw_rectangle_command rectcmd = {
          {decodeInt16(recv_buf, 1), decodeInt16(recv_buf, 3)}, // position x/y
          {decodeInt16(recv_buf, 5), decodeInt16(recv_buf, 7)}, // size w/h
          {recv_buf[9], recv_buf[10], recv_buf[11]}};           // color r/g/b

      draw_rectangle(&rectcmd);
      return 1;
    }

    break;

  case draw_character_command:

    if (size != draw_character_command_datalength) {
      SDL_LogError(
          SDL_LOG_CATEGORY_ERROR,
          "Invalid draw character packet: expected length %d, got %d\n",
          draw_character_command_datalength, size);
      dump_packet(size, recv_buf);
      return 0;
      break;
    } else {

      struct draw_character_command charcmd = {
          recv_buf[1],                                          // char
          {decodeInt16(recv_buf, 2), decodeInt16(recv_buf, 4)}, // position x/y
          {recv_buf[6], recv_buf[7], recv_buf[8]},    // foreground r/g/b
          {recv_buf[9], recv_buf[10], recv_buf[11]}}; // background r/g/b
      draw_character(&charcmd);
      return 1;
    }

    break;

  case draw_oscilloscope_waveform_command:

    if (size < draw_oscilloscope_waveform_command_mindatalength ||
        size > draw_oscilloscope_waveform_command_maxdatalength) {
      SDL_LogError(
          SDL_LOG_CATEGORY_ERROR,
          "Invalid draw oscilloscope packet: expected length between %d "
          "and %d, got "
          "%d\n",
          draw_oscilloscope_waveform_command_mindatalength,
          draw_oscilloscope_waveform_command_maxdatalength, size);
      dump_packet(size, recv_buf);
      return 0;
      break;
    } else {

      struct draw_oscilloscope_waveform_command osccmd;

      osccmd.color =
          (struct color){recv_buf[1], recv_buf[2], recv_buf[3]}; // color r/g/b
      memcpy(osccmd.waveform, &recv_buf[4], size - 4);

      osccmd.waveform_size = size - 4;

      draw_waveform(&osccmd);
      return 1;
    }

    break;

  case joypad_keypressedstate_command: {
    if (size != joypad_keypressedstate_command_datalength) {
      SDL_LogError(
          SDL_LOG_CATEGORY_ERROR,
          "Invalid joypad keypressed state packet: expected length %d, "
          "got %d\n",
          joypad_keypressedstate_command_datalength, size);
      dump_packet(size, recv_buf);
      return 0;
      break;
    }

    // nothing is done with joypad key pressed packets for now
    return 1;
    break;
  }

  case system_info_command: {
    if (size != system_info_command_datalength) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                   "Invalid system info packet: expected length %d, "
                   "got %d\n",
                   system_info_command_datalength, size);
      dump_packet(size, recv_buf);
      break;
    }

    char *hwtype[3] = {"Headless", "Beta M8", "Production M8"};

    static int system_info_printed = 0;

    if (system_info_printed == 0) {
      SDL_Log("** Hardware info ** Device type: %s, Firmware ver %d.%d.%d",
              hwtype[recv_buf[1]], recv_buf[2], recv_buf[3], recv_buf[4]);
      system_info_printed = 1;
    }

    if (recv_buf[5] == 0x01) {
      set_large_mode(1);
    } else {
      set_large_mode(0);
    }
    return 1;
    break;
  }

  default:

    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Invalid packet\n");
    dump_packet(size, recv_buf);
    return 0;
    break;
  }
  return 1;
}
