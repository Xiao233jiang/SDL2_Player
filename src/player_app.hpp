// player_app.hpp
#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <memory>
#include <atomic>

#include "play/opengl_renderer.hpp"
#include "player_core/player_state.hpp"
#include "play/audio_player.hpp"
#include "player_thread/demux_thread.hpp"
#include "player_thread/decode_thread.hpp"
#include "player_thread/video_refresh_timer.hpp"

class PlayerApp 
{
public:
    PlayerApp(const std::string& filename);
    ~PlayerApp();
    
    bool init();
    void run();
    void stop();

    OpenGLRenderer* getRenderer() const { return renderer_.get(); }
    
    // 打开新视频的方法
    void openVideo(const std::string& filename);
    
private:
    bool setupAudio();
    bool setupVideo();
    bool createThreads();
    void handleEvents();
    void handleKeyPress(SDL_Keycode key);
    bool initWithFile();
    void startPlayback(); 
    void videoRefresh();
    void cleanUp();
    
    PlayerState state_;
    std::unique_ptr<AudioPlayer> audio_player_;
    std::unique_ptr<OpenGLRenderer> renderer_;
    std::unique_ptr<DemuxThread> demux_thread_;
    std::unique_ptr<AudioDecodeThread> audio_decode_thread_;
    std::unique_ptr<VideoDecodeThread> video_decode_thread_;
    std::unique_ptr<VideoRefreshTimer> refresh_timer_;
    
    bool initialized_ = false;
};