#pragma once

#include <SDL2/SDL.h>
#include <atomic>
#include "../ffmpeg_utils/ffmpeg_headers.hpp"
#include "../player_core/player_state.hpp"
#include "../player_core/audio_resampler.hpp"

class AudioPlayer {
public:
    AudioPlayer(PlayerState* state);
    ~AudioPlayer();
    
    bool open();
    void start();
    void stop();
    void pause(bool paused);
    
    double getAudioClock() const;
    
private:
    static void audioCallback(void* userdata, Uint8* stream, int len);
    void fillAudioBuffer(Uint8* stream, int len);
    int audioDecodeFrame(uint8_t* audio_buf, int buf_size);
    
    PlayerState* state_;
    SDL_AudioDeviceID dev_id_ = 0;
    AudioResampler resampler_;
    
    // 音频缓冲区管理（类似tutorial04）
    uint8_t audio_buf_[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    unsigned int audio_buf_size_ = 0;
    unsigned int audio_buf_index_ = 0;
    
    // 音频时钟
    std::atomic<double> audio_clock_{0};
    
    // 暂停状态
    std::atomic<bool> paused_{false};
};