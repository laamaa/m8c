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

#ifndef SLIP_H_
#define SLIP_H_

#include <stdint.h>

#define SLIP_SPECIAL_BYTE_END           0xC0
#define SLIP_SPECIAL_BYTE_ESC           0xDB

#define SLIP_ESCAPED_BYTE_END           0xDC
#define SLIP_ESCAPED_BYTE_ESC           0xDD

typedef enum {
        SLIP_STATE_NORMAL = 0x00,
        SLIP_STATE_ESCAPED
} slip_state_t;

typedef struct {
        uint8_t *buf;
        uint32_t buf_size;
        int (*recv_message)(uint8_t *data, uint32_t size);
} slip_descriptor_s;

typedef struct {
        slip_state_t state;
        uint32_t size;
        const slip_descriptor_s *descriptor;
} slip_handler_s;

typedef enum {
        SLIP_NO_ERROR = 0x00,
        SLIP_ERROR_BUFFER_OVERFLOW,
        SLIP_ERROR_UNKNOWN_ESCAPED_BYTE,
        SLIP_ERROR_INVALID_PACKET
} slip_error_t;

slip_error_t slip_init(slip_handler_s *slip, const slip_descriptor_s *descriptor);
slip_error_t slip_read_byte(slip_handler_s *slip, uint8_t byte);

#endif