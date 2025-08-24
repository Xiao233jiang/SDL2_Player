#pragma once
#include <thread>
#include <atomic>
#include "../player_core/player_state.hpp"

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

    PlayerState* state_;
    std::thread thread_;
    std::atomic<bool> running_;
};