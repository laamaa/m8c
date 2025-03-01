// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT
#ifndef AUDIO_H
#define AUDIO_H

int audio_init(const char *output_device_name, unsigned int audio_buffer_size);
void toggle_audio(const char *output_device_name, unsigned int audio_buffer_size);
void process_audio();
void audio_destroy();

#endif
