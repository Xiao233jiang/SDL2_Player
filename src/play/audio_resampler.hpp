// 音频重采样类

#pragma once

#include "../ffmpeg_utils/ffmpeg_headers.hpp"

class AudioResampler 
{
public:
    AudioResampler();
    ~AudioResampler();
    
    bool init(AVCodecContext* codec_ctx, AVSampleFormat out_fmt, int out_sample_rate, int out_channels);
    int resample(AVFrame* frame, uint8_t** out_buf);
    void close();
    
private:
    SwrContext* swr_ctx_ = nullptr;
    int out_channels_ = 0;
    int out_sample_rate_ = 0;
    AVSampleFormat out_fmt_ = AV_SAMPLE_FMT_NONE;
};