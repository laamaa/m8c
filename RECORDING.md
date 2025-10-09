# M8C Screen Recording Feature

## Overview

M8C now includes built-in screen recording functionality that captures both video and audio from your M8 tracker sessions. The recordings are saved as high-quality MP4 files with H.264 video encoding and AAC audio encoding.

## Features

- **Video Recording**: Captures the M8 display at 60 FPS
- **Audio Recording**: Records the incoming audio from the M8 device (44.1 kHz stereo)
- **H.264 Encoding**: Efficient video compression with configurable quality
- **AAC Audio**: High-quality audio encoding at 192 kbps
- **MP4 Container**: Compatible with all major video players and editors
- **Automatic Filenames**: Files are named with timestamps (e.g., `m8c_recording_20250109_143022.mp4`)

## Usage

### Starting/Stopping Recording

Press **F9** to toggle recording on/off. The recording will be saved in the current working directory.

When you start recording:
- You'll see a log message: "Recording started"
- The filename will be displayed in the console

When you stop recording:
- You'll see a log message: "Recording stopped: [filename]"
- The video file will be finalized and ready to play

### Recording Controls

- **F9**: Toggle recording on/off
- Recording works only when the M8 device is connected
- Both video and audio are recorded simultaneously

## Technical Details

### Video Specifications

- **Codec**: H.264 (libx264)
- **Resolution**: Matches M8 display (320x240 for Model 01, 480x320 for Model 02)
- **Frame Rate**: 60 FPS
- **Bitrate**: 4 Mbps
- **Pixel Format**: YUV420P
- **Preset**: Medium (balanced quality/speed)
- **CRF**: 23 (high quality)

### Audio Specifications

- **Codec**: AAC
- **Sample Rate**: 44.1 kHz
- **Channels**: Stereo
- **Bitrate**: 192 kbps
- **Sample Format**: FLTP (planar float)

## Dependencies

The recording feature requires the following FFmpeg libraries:

- libavcodec (video/audio encoding)
- libavformat (MP4 container muxing)
- libavutil (utilities)
- libswscale (video format conversion)
- libswresample (audio format conversion)

These are typically included in FFmpeg development packages on most Linux distributions.

### Installing Dependencies

#### Fedora/RHEL
```bash
sudo dnf install ffmpeg-devel
```

#### Ubuntu/Debian
```bash
sudo apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev
```

#### macOS (Homebrew)
```bash
brew install ffmpeg
```

#### Arch Linux
```bash
sudo pacman -S ffmpeg
```

## Building with Recording Support

The recording feature is automatically enabled when the required FFmpeg libraries are detected during the CMake configuration:

```bash
cd build
cmake ..
make
```

If FFmpeg libraries are not found, CMake will report an error. Install the dependencies and re-run CMake.

## Implementation Details

### Architecture

The recording system consists of several components:

1. **recorder.c/h**: Core recording engine
   - Manages FFmpeg encoding contexts
   - Handles video frame conversion (BGRA → YUV420P)
   - Handles audio resampling (S16 → FLTP)
   - Manages file I/O and muxing

2. **render.c integration**: Video capture
   - Captures frames from the main SDL texture after rendering
   - Converts pixel format for FFmpeg
   - Passes frames to recorder at 60 FPS

3. **audio_sdl.c integration**: Audio capture
   - Intercepts audio data from the M8 input stream
   - Forwards audio samples to recorder
   - Maintains audio/video synchronization

4. **input.c integration**: User interface
   - Handles F9 keyboard shortcut
   - Manages recording state
   - Provides user feedback

### Performance Considerations

- Video encoding is done on the CPU (software encoding)
- Frame capture adds minimal overhead (< 1ms per frame typically)
- Audio capture is performed in the audio callback (real-time safe)
- Recording may increase CPU usage by 10-30% depending on system

### File Size Estimates

Approximate file sizes for a 1-minute recording:

- Video (4 Mbps @ 60 FPS): ~30 MB
- Audio (192 kbps stereo): ~1.4 MB
- **Total**: ~31-32 MB per minute

## Troubleshooting

### Recording doesn't start

- Ensure the M8 device is connected
- Check that FFmpeg libraries are installed
- Verify you have write permissions in the current directory
- Check the console for error messages

### No audio in recording

- Ensure audio is enabled in M8C (not muted)
- Verify the M8 audio device is properly connected
- Check audio settings in your operating system

### Video playback issues

- Ensure you have H.264 codec support in your video player
- Try VLC media player if your default player has issues
- Check that the recording finished properly (press F9 to stop)

### High CPU usage

- Recording at 60 FPS can be CPU-intensive
- Consider closing other applications while recording
- The "medium" H.264 preset balances quality and performance
- Monitor CPU temperature during extended recordings

## Known Limitations

- Recording is only available when using SDL audio backend (not with libusb audio)
- Software encoding only (no GPU acceleration)
- Recording automatically stops if the M8 device disconnects
- No pause/resume functionality (stop creates a new file)
- No visual recording indicator in the UI yet

## Future Enhancements

Potential improvements for future versions:

- [ ] Visual recording indicator (red dot icon)
- [ ] Configurable video quality settings
- [ ] GPU-accelerated encoding (VAAPI, NVENC, VideoToolbox)
- [ ] Recording pause/resume functionality
- [ ] Custom output directory selection
- [ ] Real-time bitrate display
- [ ] Configurable frame rate options

## Credits

Screen recording implementation by the M8C development team.
Built with FFmpeg libraries and SDL3.

## License

This recording feature is released under the MIT License, consistent with the rest of M8C.

