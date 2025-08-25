#include "audio_player.hpp"
#include <iostream>
#include <cstring>

AudioPlayer::AudioPlayer(PlayerState* state) 
    : state_(state)
{
    memset(audio_buf_, 0, sizeof(audio_buf_));
}

AudioPlayer::~AudioPlayer() 
{
    stop();
}

bool AudioPlayer::open() 
{
    if (dev_id_ > 0) 
    {
        return false;
    }
    
    if (!state_->audio_ctx) 
    {
        return false;
    }
    
    SDL_AudioSpec wanted = {}, obtained = {};
    wanted.freq = state_->audio_ctx->sample_rate;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = state_->audio_ctx->channels;
    wanted.silence = 0;
    wanted.samples = SDL_AUDIO_BUFFER_SIZE;
    wanted.callback = audioCallback;
    wanted.userdata = this;
    
    dev_id_ = SDL_OpenAudioDevice(nullptr, 0, &wanted, &obtained, 0);
    if (dev_id_ == 0) 
    {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // 初始化重采样器
    if (!resampler_.init(state_->audio_ctx, 
                        AV_SAMPLE_FMT_S16,
                        obtained.freq, 
                        obtained.channels)) 
    {
        std::cerr << "Failed to initialize audio resampler" << std::endl;
        return false;
    }
    
    return true;
}

void AudioPlayer::start() 
{
    if (dev_id_ > 0) 
    {
        SDL_PauseAudioDevice(dev_id_, 0);
        paused_ = false;
    }
}

void AudioPlayer::stop() 
{
    if (dev_id_ > 0) 
    {
        SDL_PauseAudioDevice(dev_id_, 1);
        SDL_CloseAudioDevice(dev_id_);
        dev_id_ = 0;
    }
}

void AudioPlayer::pause(bool paused)
{
    if (dev_id_ > 0) 
    {
        SDL_PauseAudioDevice(dev_id_, paused ? 1 : 0);
        paused_ = paused;
    }
}

double AudioPlayer::getAudioClock() const
{
    return state_->audio_clock.get();
}

void AudioPlayer::audioCallback(void* userdata, Uint8* stream, int len) 
{
    auto* player = static_cast<AudioPlayer*>(userdata);
    player->fillAudioBuffer(stream, len);
}

void AudioPlayer::fillAudioBuffer(Uint8* stream, int len) 
{
    if (paused_) 
    {
        memset(stream, 0, len);
        return;
    }

    int len1 = 0;
    int audio_size = 0;

    while (len > 0 && !state_->quit.load()) 
    {
        if (audio_buf_index_ >= audio_buf_size_) 
        {
            // 需要获取更多数据
            audio_size = audioProcessFrame(audio_buf_, sizeof(audio_buf_));
            
            if (audio_size < 0) 
            {
                // 出错时输出静音
                audio_buf_size_ = 1024;
                memset(audio_buf_, 0, audio_buf_size_);
            } 
            else 
            {
                audio_buf_size_ = audio_size;
            }
            audio_buf_index_ = 0;
        }
        
        len1 = audio_buf_size_ - audio_buf_index_;
        if (len1 > len) 
        {
            len1 = len;
        }
        
        memcpy(stream, audio_buf_ + audio_buf_index_, len1);
        len -= len1;
        stream += len1;
        audio_buf_index_ += len1;
    }
}

int AudioPlayer::audioProcessFrame(uint8_t* audio_buf, int buf_size) 
{
    if (state_->quit.load()) 
    {
        return -1;
    }

    AVFrame* frame = nullptr;
    if (!state_->audio_frame_queue.pop(frame, state_->quit, 10)) 
    {
        return -1; // 超时或退出
    }
    
    // 添加安全检查 - 确保frame和音频上下文有效
    if (!frame || !state_->audio_ctx || state_->audio_ctx->sample_rate <= 0) {
        if (frame) av_frame_free(&frame);
        memset(audio_buf, 0, buf_size);
        return buf_size; // 返回静音
    }

    double pts = NAN;
    if (frame->pts != AV_NOPTS_VALUE) 
    {
        pts = frame->pts * av_q2d(state_->audio_ctx->time_base);
    }

    // 检查帧是否有有效的样本数
    if (frame->nb_samples <= 0) 
    {
        av_frame_free(&frame);
        memset(audio_buf, 0, buf_size);
        return buf_size; // 返回静音
    }

    // 保存样本数，因为frame将在重采样后被释放
    int nb_samples = frame->nb_samples;
    
    uint8_t* resampled_buf = nullptr;
    int data_size = resampler_.resample(frame, &resampled_buf);
    av_frame_free(&frame); // 尽早释放 frame

    if (data_size <= 0) 
    {
        if (resampled_buf) 
        {
            av_freep(&resampled_buf);
        }
        // 返回静音
        memset(audio_buf, 0, buf_size);
        return buf_size;
    }

    if (data_size > buf_size) 
    {
        // 日志警告，但尽量拷贝有效部分
        data_size = buf_size;
    }

    memcpy(audio_buf, resampled_buf, data_size);
    av_freep(&resampled_buf);

    // 更新时间戳和统计信息
    if (!std::isnan(pts)) 
    {
        // 计算音频持续时间 - 使用之前保存的样本数
        double duration = 0.0;
        if (state_->audio_ctx->sample_rate > 0) 
        {
            duration = (double)nb_samples / (double)state_->audio_ctx->sample_rate;
        }
        
        // 更新音频时钟 - 使用 Clock 类
        state_->audio_clock.set(pts + duration);
        // 更新统计信息
        state_->stats.audio_bytes.fetch_add(data_size);
    }

    return data_size;
}