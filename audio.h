#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

//Recording/playback callbacks
void audio_cb_in( void* userdata, uint8_t* stream, int len );
void audio_cb_out( void* userdata, uint8_t* stream, int len );

int audio_init(int audio_buffer_size);
void audio_destroy();

#endif
