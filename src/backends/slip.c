/*
MIT License

Copyright (c) 2018 Marcin Borowicz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/* This code is originally by marcinbor85, https://github.com/marcinbor85/slip
It has been simplified a bit as CRC checking etc. is not required in this
program. */

#include "slip.h"

#include <assert.h>
#include <stddef.h>

static void reset_rx(slip_handler_s *slip) {
  assert(slip != NULL);

  slip->state = SLIP_STATE_NORMAL;
  slip->size = 0;
}

slip_error_t slip_init(slip_handler_s *slip, const slip_descriptor_s *descriptor) {
  assert(slip != NULL);
  assert(descriptor != NULL);
  assert(descriptor->buf != NULL);
  assert(descriptor->recv_message != NULL);

  slip->descriptor = descriptor;
  reset_rx(slip);

  return SLIP_NO_ERROR;
}

static slip_error_t put_byte_to_buffer(slip_handler_s *slip, const uint8_t byte) {
  slip_error_t error = SLIP_NO_ERROR;

  assert(slip != NULL);

  if (slip->size >= slip->descriptor->buf_size) {
    error = SLIP_ERROR_BUFFER_OVERFLOW;
    reset_rx(slip);
  } else {
    slip->descriptor->buf[slip->size++] = byte;
    slip->state = SLIP_STATE_NORMAL;
  }

  return error;
}

slip_error_t slip_read_byte(slip_handler_s *slip, uint8_t byte) {
  slip_error_t error = SLIP_NO_ERROR;

  assert(slip != NULL);

  switch (slip->state) {
  case SLIP_STATE_NORMAL:
    switch (byte) {
    case SLIP_SPECIAL_BYTE_END:
      if (!slip->descriptor->recv_message(slip->descriptor->buf, slip->size)) {
        error = SLIP_ERROR_INVALID_PACKET;
      }
      reset_rx(slip);
      break;
    case SLIP_SPECIAL_BYTE_ESC:
      slip->state = SLIP_STATE_ESCAPED;
      break;
    default:
      error = put_byte_to_buffer(slip, byte);
      break;
    }
    break;

  case SLIP_STATE_ESCAPED:
    switch (byte) {
    case SLIP_ESCAPED_BYTE_END:
      byte = SLIP_SPECIAL_BYTE_END;
      break;
    case SLIP_ESCAPED_BYTE_ESC:
      byte = SLIP_SPECIAL_BYTE_ESC;
      break;
    default:
      error = SLIP_ERROR_UNKNOWN_ESCAPED_BYTE;
      reset_rx(slip);
      break;
    }

    if (error != SLIP_NO_ERROR)
      break;

    error = put_byte_to_buffer(slip, byte);
    break;
  }

  return error;
}
