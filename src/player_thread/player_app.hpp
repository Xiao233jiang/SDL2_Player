#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <memory>
#include <atomic>

#include "../player_core/player_state.hpp"
#include "audio_player.hpp"
#include "../renderer/renderer.hpp"
#include "demux_thread.hpp"
#include "decode_thread.hpp"
#include "video_refresh_timer.hpp"

class PlayerApp {
public:
    PlayerApp(const std::string& filename);
    ~PlayerApp();
    
    bool init();
    void run();
    void stop();
    
private:
    bool initSDL();
    bool setupAudio();
    bool setupVideo();
    bool createThreads();
    void handleEvents();
    void videoRefresh();
    void cleanUp();
    
    PlayerState state_;
    std::unique_ptr<AudioPlayer> audio_player_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<DemuxThread> demux_thread_;
    std::unique_ptr<AudioDecodeThread> audio_decode_thread_;
    std::unique_ptr<VideoDecodeThread> video_decode_thread_;
    std::unique_ptr<VideoRefreshTimer> refresh_timer_;
    
    bool initialized_ = false;
};