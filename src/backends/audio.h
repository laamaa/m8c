// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT
#ifndef AUDIO_H
#define AUDIO_H

int audio_initialize(const char *output_device_name, unsigned int audio_buffer_size);
void audio_toggle(const char *output_device_name, unsigned int audio_buffer_size);
void audio_process(void);
void audio_close(void);

#endif
