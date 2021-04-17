#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "command.h"
#include "render.h"

#define DEBUG 1

// Convert 2 little-endian 8bit bytes to a 16bit integer
static uint16_t decodeInt16(uint8_t *data, uint8_t start) {
  return data[start] | (uint16_t)data[start + 1] << 8;
}

enum {
  draw_rectangle_command = 0xFE,
  draw_rectangle_command_datalength = 12,
  draw_character_command = 0xFD,
  draw_character_command_datalength = 12,
  draw_oscilloscope_waveform_command = 0xFC,
  draw_oscilloscope_waveform_command_mindatalength = 1 + 3,
  draw_oscilloscope_waveform_command_maxdatalength = 1 + 3 + 320,
  joypad_keypressedstate_command = 0xFB,
  joypad_keypressedstate_command_datalength = 2
};

void process_command(uint8_t *data, uint32_t size,
                     struct command_queues *command_queues) {

  uint8_t recv_buf[size + 1];

  memcpy(recv_buf, data, size);
  recv_buf[size] = 0;
#if DEBUG > 2
  fprintf(stderr, "RECV size %d: ", size);
  for (int i = 0; i < size; i++) {
    fprintf(stderr, "0x%02X ", recv_buf[i]);
  }
  fprintf(stderr, "\n");
#endif

  switch (recv_buf[0]) {

  case draw_rectangle_command:
    if (size != draw_rectangle_command_datalength) {
#if DEBUG > 0
      fprintf(stderr,
              "Invalid draw rectangle packet: expected length %d, got %d\n",
              draw_rectangle_command_datalength, size);
      for (uint16_t a = 0; a < size; a++) {
        fprintf(stderr, "0x%02X ", recv_buf[a]);
      }
      fprintf(stderr, "\n");
#endif
      break;
    }
    struct draw_rectangle_command rectcmd = {
        {decodeInt16(recv_buf, 1), decodeInt16(recv_buf, 3)}, // position x/y
        {decodeInt16(recv_buf, 5), decodeInt16(recv_buf, 7)}, // size w/h
        {recv_buf[9], recv_buf[10], recv_buf[11]}};           // color r/g/b
        draw_rectangle(&rectcmd);
    // command_queues->rectangles[command_queues->rectangles_queue_size++] =
    //     rectcmd;
#if DEBUG > 1
    fprintf(
        stderr, "Rectangle: x: %d, y: %d, w: %d, h: %d, r: %d, g: %d, b: %d\n",
        rectcmd.pos.x, rectcmd.pos.y, rectcmd.size.width, rectcmd.size.height,
        rectcmd.color.r, rectcmd.color.g, rectcmd.color.b);
#endif

    break;

  case draw_character_command:
    if (size != draw_character_command_datalength) {
#if DEBUG > 0
      fprintf(stderr,
              "Invalid draw character packet: expected length %d, got %d\n",
              draw_character_command_datalength, size);
      for (uint16_t a = 0; a < size; a++) {
        fprintf(stderr, "0x%02X ", recv_buf[a]);
      }
      fprintf(stderr, "\n");
      break;
#endif
    }
    struct draw_character_command charcmd = {
        recv_buf[1],                                          // char
        {decodeInt16(recv_buf, 2), decodeInt16(recv_buf, 4)}, // position x/y
        {recv_buf[6], recv_buf[7], recv_buf[8]},    // foreground r/g/b
        {recv_buf[9], recv_buf[10], recv_buf[11]}}; // background r/g/b
        draw_character(&charcmd);
    // command_queues->characters[command_queues->characters_queue_size++] =
    //     charcmd;
    break;

  case draw_oscilloscope_waveform_command:
    if (size < draw_oscilloscope_waveform_command_mindatalength) {
#if DEBUG > 0
      fprintf(stderr,
              "Invalid draw oscilloscope packet: expected min length %d, got "
              "%d\n",
              draw_oscilloscope_waveform_command_mindatalength, size);
      for (uint16_t a = 0; a < size; a++) {
        fprintf(stderr, "0x%02X ", recv_buf[a]);
      }
      fprintf(stderr, "\n");
#endif
      break;
    }
    if (size > draw_oscilloscope_waveform_command_maxdatalength) {
#if DEBUG > 0
      fprintf(stderr,
              "Invalid draw oscilloscope packet: expected max length %d, got "
              "%d\n",
              draw_oscilloscope_waveform_command_maxdatalength, size);
      for (uint16_t a = 0; a < size; a++) {
        fprintf(stderr, "0x%02X ", recv_buf[a]);
      }
      fprintf(stderr, "\n");
#endif
      break;
    }
    struct draw_oscilloscope_waveform_command osccmd;
    osccmd.color =
        (struct color){recv_buf[1], recv_buf[2], recv_buf[3]}; // color r/g/b
    memcpy(osccmd.waveform, &recv_buf[4], size - 4);
    osccmd.waveform_size = size - 4;
    draw_waveform(&osccmd);
    // // only 1 oscilloscope command gets queued for now
    // command_queues->waveform = osccmd;
    // command_queues->waveforms_queue_size = 1;
#if DEBUG > 1
    fprintf(stderr, "Oscilloscope: r: %d, g: %d, b: %d, data: ", osccmd.color.r,
            osccmd.color.g, osccmd.color.b);
    if (osccmd.waveform_size == 0) {
      fprintf(stderr, "-");
    } else {
      for (int w = 0; w < osccmd.waveform_size; w++) {
        fprintf(stderr, "0x%02X", osccmd.waveform[w]);
      }
    }
    fprintf(stderr, "\n");
#endif
    break;

  case joypad_keypressedstate_command:
    if (size != joypad_keypressedstate_command_datalength) {
#if DEBUG > 0
      fprintf(stderr,
              "Invalid joypad keypressed state packet: expected length %d, "
              "got %d\n",
              joypad_keypressedstate_command_datalength, size);
      break;
#endif
    }
    break;

  default:
    fprintf(stderr, "Invalid packet\n");
    for (uint16_t a = 0; a < size; a++) {
      fprintf(stderr, "0x%02X ", recv_buf[a]);
    }
    fprintf(stderr, "\n");
    break;
  }
}