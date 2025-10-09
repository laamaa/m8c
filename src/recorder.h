// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef RECORDER_H_
#define RECORDER_H_

#include <stdint.h>

// Initialize the recording system
int recorder_init(void);

// Start recording with given framebuffer dimensions and framerate
// Creates a new video file with timestamp in the filename
int recorder_start(int width, int height, int fps);

// Stop recording and finalize the video file
void recorder_stop(void);

// Check if currently recording
int recorder_is_recording(void);

// Add a video frame (ARGB8888 format from SDL texture)
void recorder_add_video_frame(const uint8_t *pixels, int width, int height, int pitch);

// Add audio samples (S16LE stereo format, 44100 Hz)
void recorder_add_audio_samples(const int16_t *samples, int num_samples);

// Cleanup recording system
void recorder_close(void);

// Toggle recording on/off
void recorder_toggle(int width, int height, int fps);

#endif // RECORDER_H_

