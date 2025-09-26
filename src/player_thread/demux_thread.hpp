#pragma once
#include <thread>
#include <atomic>
#include "../player_core/player_state.hpp"
#include "../player_core/utils/player_constants.hpp" // ✅ 包含常量定义

class DemuxThread 
{
public:
    DemuxThread(PlayerState* state)
        : state_(state), running_(false) 
    {
    }

    void start();
    void join();
    void stop();

private:
    void run();
    bool handleSeekRequest();

    PlayerState* state_;
    std::thread thread_;
    std::atomic<bool> running_;
};