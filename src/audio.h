// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT
#ifndef AUDIO_H
#define AUDIO_H

int audio_init(unsigned int audio_buffer_size, const char *output_device_name);
void toggle_audio(unsigned int audio_buffer_size, const char *output_device_name);
void audio_destroy();

#endif
