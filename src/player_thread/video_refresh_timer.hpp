#pragma once
#include <thread>
#include <atomic>
#include <SDL2/SDL.h>
#include "../player_core/player_state.hpp"
#include "../player_core/utils/player_constants.hpp"

class VideoRefreshTimer 
{
public:
    VideoRefreshTimer(PlayerState* state, int interval_ms = DEFAULT_VIDEO_INTERVAL_MS);
    ~VideoRefreshTimer();

    void start();
    void join();
    void stop();
    
    void setInterval(int interval_ms);
    int getInterval() const;

private:
    void run();

    PlayerState* state_;
    int interval_ms_;
    std::thread thread_;
    std::atomic<bool> running_;
};