#include "audio_resampler.hpp"
#include <iostream>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
}

AudioResampler::AudioResampler() = default;

AudioResampler::~AudioResampler() 
{
    close();
}

bool AudioResampler::init(AVCodecContext* codec_ctx, AVSampleFormat out_fmt, int out_sample_rate, int out_channels) 
{
    close();

    out_fmt_ = out_fmt;
    out_sample_rate_ = out_sample_rate;
    out_channels_ = out_channels;

    // 使用老版 API，直接用 channel_layout
    int64_t in_ch_layout = codec_ctx->channel_layout;
    if (in_ch_layout == 0) 
    {
        // 如果未指定，使用默认布局
        in_ch_layout = av_get_default_channel_layout(codec_ctx->channels);
    }
    int64_t out_ch_layout = av_get_default_channel_layout(out_channels);

    swr_ctx_ = swr_alloc_set_opts(
        nullptr,
        out_ch_layout, out_fmt, out_sample_rate,
        in_ch_layout, codec_ctx->sample_fmt, codec_ctx->sample_rate,
        0, nullptr
    );
    if (!swr_ctx_ || swr_init(swr_ctx_) < 0) 
    {
        close();
        return false;
    }
    return true;
}

int AudioResampler::resample(AVFrame* frame, uint8_t** out_buf) 
{
    if (!swr_ctx_ || !frame) return -1;

    // 计算输出样本数
    int64_t delay = swr_get_delay(swr_ctx_, frame->sample_rate);
    int64_t out_samples = av_rescale_rnd(delay + frame->nb_samples,
                                        out_sample_rate_,
                                        frame->sample_rate,
                                        AV_ROUND_UP);

    if (out_samples <= 0) {
        std::cerr << "Invalid output samples: " << out_samples << std::endl;
        return -1;
    }

    int out_linesize = 0;
    int ret = av_samples_alloc(out_buf, &out_linesize, out_channels_, 
                             out_samples, out_fmt_, 0);
    if (ret < 0) {
        std::cerr << "Failed to allocate samples: " << ret << std::endl;
        return -1;
    }

    // 执行重采样
    int converted_samples = swr_convert(swr_ctx_, 
                                       out_buf, out_samples,
                                       (const uint8_t**)frame->data, frame->nb_samples);
    
    if (converted_samples < 0) {
        std::cerr << "swr_convert failed: " << converted_samples << std::endl;
        av_freep(out_buf);
        return -1;
    }

    // 计算实际数据大小
    int data_size = av_samples_get_buffer_size(&out_linesize, out_channels_, 
                                             converted_samples, out_fmt_, 1);
    if (data_size < 0) {
        std::cerr << "Failed to get buffer size: " << data_size << std::endl;
        av_freep(out_buf);
        return -1;
    }

    return data_size;
}

void AudioResampler::close() 
{
    if (swr_ctx_) 
    {
        swr_free(&swr_ctx_);
        swr_ctx_ = nullptr;
    }
}