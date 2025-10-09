// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include "recorder.h"

#include <SDL3/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

// Recording state
static struct {
    AVFormatContext *format_ctx;
    
    // Video encoding
    AVStream *video_stream;
    AVCodecContext *video_codec_ctx;
    const AVCodec *video_codec;
    AVFrame *video_frame;
    AVPacket *video_packet;
    struct SwsContext *sws_ctx;
    int64_t video_pts;
    
    // Audio encoding
    AVStream *audio_stream;
    AVCodecContext *audio_codec_ctx;
    const AVCodec *audio_codec;
    AVFrame *audio_frame;
    AVPacket *audio_packet;
    struct SwrContext *swr_ctx;
    int64_t audio_pts;
    uint8_t **audio_buffer;
    int audio_buffer_size;
    int audio_samples_in_buffer;
    
    // Recording parameters
    int width;
    int height;
    int fps;
    int is_recording;
    
    char output_filename[256];
} recorder_state = {0};

int recorder_init(void) {
    SDL_memset(&recorder_state, 0, sizeof(recorder_state));
    SDL_Log("Recorder initialized");
    return 1;
}

static int init_video_encoder(int width, int height, int fps) {
    // Find H.264 encoder
    recorder_state.video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!recorder_state.video_codec) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "H.264 codec not found");
        return 0;
    }
    
    // Create video stream
    recorder_state.video_stream = avformat_new_stream(recorder_state.format_ctx, NULL);
    if (!recorder_state.video_stream) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not create video stream");
        return 0;
    }
    recorder_state.video_stream->id = 0;
    
    // Allocate codec context
    recorder_state.video_codec_ctx = avcodec_alloc_context3(recorder_state.video_codec);
    if (!recorder_state.video_codec_ctx) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not allocate video codec context");
        return 0;
    }
    
    // Set encoding parameters
    recorder_state.video_codec_ctx->codec_id = AV_CODEC_ID_H264;
    recorder_state.video_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    recorder_state.video_codec_ctx->width = width;
    recorder_state.video_codec_ctx->height = height;
    recorder_state.video_codec_ctx->time_base = (AVRational){1, fps};
    recorder_state.video_codec_ctx->framerate = (AVRational){fps, 1};
    recorder_state.video_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    recorder_state.video_codec_ctx->bit_rate = 4000000; // 4 Mbps
    recorder_state.video_codec_ctx->gop_size = fps; // One keyframe per second
    recorder_state.video_codec_ctx->max_b_frames = 2;
    
    // Set H.264 specific options for better quality
    av_opt_set(recorder_state.video_codec_ctx->priv_data, "preset", "medium", 0);
    av_opt_set(recorder_state.video_codec_ctx->priv_data, "crf", "23", 0);
    
    // If this is MP4, set required flags
    if (recorder_state.format_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        recorder_state.video_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    // Open codec
    if (avcodec_open2(recorder_state.video_codec_ctx, recorder_state.video_codec, NULL) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not open video codec");
        return 0;
    }
    
    // Copy codec parameters to stream
    if (avcodec_parameters_from_context(recorder_state.video_stream->codecpar, 
                                         recorder_state.video_codec_ctx) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not copy codec parameters to stream");
        return 0;
    }
    
    recorder_state.video_stream->time_base = recorder_state.video_codec_ctx->time_base;
    
    // Allocate video frame
    recorder_state.video_frame = av_frame_alloc();
    if (!recorder_state.video_frame) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not allocate video frame");
        return 0;
    }
    recorder_state.video_frame->format = recorder_state.video_codec_ctx->pix_fmt;
    recorder_state.video_frame->width = width;
    recorder_state.video_frame->height = height;
    
    if (av_frame_get_buffer(recorder_state.video_frame, 32) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not allocate video frame buffer");
        return 0;
    }
    
    // Allocate packet
    recorder_state.video_packet = av_packet_alloc();
    if (!recorder_state.video_packet) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not allocate video packet");
        return 0;
    }
    
    // Initialize SWS context for pixel format conversion (ARGB -> YUV420P)
    recorder_state.sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_BGRA,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BICUBIC, NULL, NULL, NULL
    );
    
    if (!recorder_state.sws_ctx) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not initialize SWS context");
        return 0;
    }
    
    recorder_state.video_pts = 0;
    
    SDL_Log("Video encoder initialized: %dx%d @ %d fps", width, height, fps);
    return 1;
}

static int init_audio_encoder(void) {
    // Find AAC encoder
    recorder_state.audio_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!recorder_state.audio_codec) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "AAC codec not found");
        return 0;
    }
    
    // Create audio stream
    recorder_state.audio_stream = avformat_new_stream(recorder_state.format_ctx, NULL);
    if (!recorder_state.audio_stream) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Could not create audio stream");
        return 0;
    }
    recorder_state.audio_stream->id = 1;
    
    // Allocate codec context
    recorder_state.audio_codec_ctx = avcodec_alloc_context3(recorder_state.audio_codec);
    if (!recorder_state.audio_codec_ctx) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Could not allocate audio codec context");
        return 0;
    }
    
    // Set encoding parameters
    recorder_state.audio_codec_ctx->codec_id = AV_CODEC_ID_AAC;
    recorder_state.audio_codec_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    recorder_state.audio_codec_ctx->sample_rate = 44100;
    recorder_state.audio_codec_ctx->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
    recorder_state.audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP; // AAC typically uses planar float
    recorder_state.audio_codec_ctx->bit_rate = 192000; // 192 kbps
    recorder_state.audio_codec_ctx->time_base = (AVRational){1, 44100};
    
    // If this is MP4, set required flags
    if (recorder_state.format_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        recorder_state.audio_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    // Open codec
    if (avcodec_open2(recorder_state.audio_codec_ctx, recorder_state.audio_codec, NULL) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Could not open audio codec");
        return 0;
    }
    
    // Copy codec parameters to stream
    if (avcodec_parameters_from_context(recorder_state.audio_stream->codecpar, 
                                         recorder_state.audio_codec_ctx) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Could not copy codec parameters to stream");
        return 0;
    }
    
    recorder_state.audio_stream->time_base = recorder_state.audio_codec_ctx->time_base;
    
    // Allocate audio frame
    recorder_state.audio_frame = av_frame_alloc();
    if (!recorder_state.audio_frame) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Could not allocate audio frame");
        return 0;
    }
    recorder_state.audio_frame->format = recorder_state.audio_codec_ctx->sample_fmt;
    recorder_state.audio_frame->ch_layout = recorder_state.audio_codec_ctx->ch_layout;
    recorder_state.audio_frame->sample_rate = recorder_state.audio_codec_ctx->sample_rate;
    recorder_state.audio_frame->nb_samples = recorder_state.audio_codec_ctx->frame_size;
    
    if (av_frame_get_buffer(recorder_state.audio_frame, 0) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Could not allocate audio frame buffer");
        return 0;
    }
    
    // Allocate packet
    recorder_state.audio_packet = av_packet_alloc();
    if (!recorder_state.audio_packet) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Could not allocate audio packet");
        return 0;
    }
    
    // Initialize resampler (S16 stereo -> FLTP stereo)
    if (swr_alloc_set_opts2(&recorder_state.swr_ctx,
                            &recorder_state.audio_codec_ctx->ch_layout,
                            recorder_state.audio_codec_ctx->sample_fmt,
                            recorder_state.audio_codec_ctx->sample_rate,
                            &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO,
                            AV_SAMPLE_FMT_S16,
                            44100,
                            0, NULL) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Could not allocate resampler");
        return 0;
    }
    
    if (swr_init(recorder_state.swr_ctx) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Could not initialize resampler");
        return 0;
    }
    
    // Allocate audio buffer for accumulating samples
    recorder_state.audio_buffer_size = recorder_state.audio_codec_ctx->frame_size;
    av_samples_alloc_array_and_samples(&recorder_state.audio_buffer, NULL, 2, 
                                       recorder_state.audio_buffer_size, AV_SAMPLE_FMT_S16, 0);
    recorder_state.audio_samples_in_buffer = 0;
    
    recorder_state.audio_pts = 0;
    
    SDL_Log("Audio encoder initialized: 44100 Hz stereo AAC @ 192 kbps");
    return 1;
}

int recorder_start(int width, int height, int fps) {
    if (recorder_state.is_recording) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Already recording");
        return 0;
    }
    
    recorder_state.width = width;
    recorder_state.height = height;
    recorder_state.fps = fps;
    
    // Generate output filename with timestamp
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);
    SDL_snprintf(recorder_state.output_filename, sizeof(recorder_state.output_filename),
                "m8c_recording_%s.mp4", timestamp);
    
    // Allocate output context
    if (avformat_alloc_output_context2(&recorder_state.format_ctx, NULL, "mp4", 
                                        recorder_state.output_filename) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create output context");
        return 0;
    }
    
    // Initialize video encoder
    if (!init_video_encoder(width, height, fps)) {
        recorder_stop();
        return 0;
    }
    
    // Initialize audio encoder
    if (!init_audio_encoder()) {
        recorder_stop();
        return 0;
    }
    
    // Open output file
    if (!(recorder_state.format_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&recorder_state.format_ctx->pb, recorder_state.output_filename, 
                      AVIO_FLAG_WRITE) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not open output file: %s", 
                        recorder_state.output_filename);
            recorder_stop();
            return 0;
        }
    }
    
    // Write file header
    if (avformat_write_header(recorder_state.format_ctx, NULL) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not write file header");
        recorder_stop();
        return 0;
    }
    
    recorder_state.is_recording = 1;
    SDL_Log("Recording started: %s", recorder_state.output_filename);
    
    return 1;
}

static void encode_video_frame(AVFrame *frame) {
    int ret;
    
    if (frame) {
        frame->pts = recorder_state.video_pts++;
    }
    
    // Send frame to encoder
    ret = avcodec_send_frame(recorder_state.video_codec_ctx, frame);
    if (ret < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Error sending video frame to encoder");
        return;
    }
    
    // Receive encoded packets
    while (ret >= 0) {
        ret = avcodec_receive_packet(recorder_state.video_codec_ctx, recorder_state.video_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Error receiving video packet from encoder");
            return;
        }
        
        // Rescale packet timestamps
        av_packet_rescale_ts(recorder_state.video_packet, 
                            recorder_state.video_codec_ctx->time_base,
                            recorder_state.video_stream->time_base);
        recorder_state.video_packet->stream_index = recorder_state.video_stream->index;
        
        // Write packet to file
        ret = av_interleaved_write_frame(recorder_state.format_ctx, recorder_state.video_packet);
        if (ret < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Error writing video packet");
        }
        
        av_packet_unref(recorder_state.video_packet);
    }
}

static void encode_audio_frame(AVFrame *frame) {
    int ret;
    
    if (frame) {
        frame->pts = recorder_state.audio_pts;
        recorder_state.audio_pts += frame->nb_samples;
    }
    
    // Send frame to encoder
    ret = avcodec_send_frame(recorder_state.audio_codec_ctx, frame);
    if (ret < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Error sending audio frame to encoder");
        return;
    }
    
    // Receive encoded packets
    while (ret >= 0) {
        ret = avcodec_receive_packet(recorder_state.audio_codec_ctx, recorder_state.audio_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Error receiving audio packet from encoder");
            return;
        }
        
        // Rescale packet timestamps
        av_packet_rescale_ts(recorder_state.audio_packet, 
                            recorder_state.audio_codec_ctx->time_base,
                            recorder_state.audio_stream->time_base);
        recorder_state.audio_packet->stream_index = recorder_state.audio_stream->index;
        
        // Write packet to file
        ret = av_interleaved_write_frame(recorder_state.format_ctx, recorder_state.audio_packet);
        if (ret < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Error writing audio packet");
        }
        
        av_packet_unref(recorder_state.audio_packet);
    }
}

void recorder_add_video_frame(const uint8_t *pixels, int width, int height, int pitch) {
    if (!recorder_state.is_recording || !pixels) {
        return;
    }
    
    // Make frame writable
    if (av_frame_make_writable(recorder_state.video_frame) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not make video frame writable");
        return;
    }
    
    // Convert ARGB pixels to YUV420P
    const uint8_t *src_data[1] = { pixels };
    int src_linesize[1] = { pitch };
    
    sws_scale(recorder_state.sws_ctx, src_data, src_linesize, 0, height,
              recorder_state.video_frame->data, recorder_state.video_frame->linesize);
    
    encode_video_frame(recorder_state.video_frame);
}

void recorder_add_audio_samples(const int16_t *samples, int num_samples) {
    if (!recorder_state.is_recording || !samples || num_samples <= 0) {
        return;
    }
    
    int samples_to_process = num_samples;
    int sample_offset = 0;
    
    while (samples_to_process > 0) {
        int samples_to_copy = recorder_state.audio_buffer_size - recorder_state.audio_samples_in_buffer;
        if (samples_to_copy > samples_to_process) {
            samples_to_copy = samples_to_process;
        }
        
        // Copy samples to buffer (interleaved stereo)
        SDL_memcpy((int16_t*)recorder_state.audio_buffer[0] + 
                  (recorder_state.audio_samples_in_buffer * 2),
                  samples + (sample_offset * 2),
                  samples_to_copy * 2 * sizeof(int16_t));
        
        recorder_state.audio_samples_in_buffer += samples_to_copy;
        samples_to_process -= samples_to_copy;
        sample_offset += samples_to_copy;
        
        // If buffer is full, encode frame
        if (recorder_state.audio_samples_in_buffer >= recorder_state.audio_buffer_size) {
            // Make frame writable
            if (av_frame_make_writable(recorder_state.audio_frame) < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Could not make audio frame writable");
                return;
            }
            
            // Resample from S16 interleaved to FLTP planar
            const uint8_t *in_data[1] = { (uint8_t*)recorder_state.audio_buffer[0] };
            swr_convert(recorder_state.swr_ctx, 
                       recorder_state.audio_frame->data, 
                       recorder_state.audio_frame->nb_samples,
                       in_data, 
                       recorder_state.audio_samples_in_buffer);
            
            encode_audio_frame(recorder_state.audio_frame);
            recorder_state.audio_samples_in_buffer = 0;
        }
    }
}

void recorder_stop(void) {
    if (!recorder_state.is_recording) {
        return;
    }
    
    SDL_Log("Stopping recording...");
    
    // Flush video encoder
    if (recorder_state.video_codec_ctx) {
        encode_video_frame(NULL);
    }
    
    // Flush audio encoder
    if (recorder_state.audio_codec_ctx) {
        encode_audio_frame(NULL);
    }
    
    // Write file trailer
    if (recorder_state.format_ctx) {
        av_write_trailer(recorder_state.format_ctx);
    }
    
    // Cleanup video resources
    if (recorder_state.sws_ctx) {
        sws_freeContext(recorder_state.sws_ctx);
        recorder_state.sws_ctx = NULL;
    }
    if (recorder_state.video_frame) {
        av_frame_free(&recorder_state.video_frame);
    }
    if (recorder_state.video_packet) {
        av_packet_free(&recorder_state.video_packet);
    }
    if (recorder_state.video_codec_ctx) {
        avcodec_free_context(&recorder_state.video_codec_ctx);
    }
    
    // Cleanup audio resources
    if (recorder_state.swr_ctx) {
        swr_free(&recorder_state.swr_ctx);
    }
    if (recorder_state.audio_buffer) {
        av_freep(&recorder_state.audio_buffer[0]);
        av_freep(&recorder_state.audio_buffer);
    }
    if (recorder_state.audio_frame) {
        av_frame_free(&recorder_state.audio_frame);
    }
    if (recorder_state.audio_packet) {
        av_packet_free(&recorder_state.audio_packet);
    }
    if (recorder_state.audio_codec_ctx) {
        avcodec_free_context(&recorder_state.audio_codec_ctx);
    }
    
    // Close output file
    if (recorder_state.format_ctx) {
        if (!(recorder_state.format_ctx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&recorder_state.format_ctx->pb);
        }
        avformat_free_context(recorder_state.format_ctx);
        recorder_state.format_ctx = NULL;
    }
    
    recorder_state.is_recording = 0;
    SDL_Log("Recording stopped: %s", recorder_state.output_filename);
}

int recorder_is_recording(void) {
    return recorder_state.is_recording;
}

void recorder_toggle(int width, int height, int fps) {
    if (recorder_state.is_recording) {
        recorder_stop();
    } else {
        recorder_start(width, height, fps);
    }
}

void recorder_close(void) {
    if (recorder_state.is_recording) {
        recorder_stop();
    }
    SDL_Log("Recorder closed");
}

