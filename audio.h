// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

void audio_cb_in(void *userdata, uint8_t *stream, int len);
int audio_init(int audio_buffer_size);
void audio_destroy();

#endif
