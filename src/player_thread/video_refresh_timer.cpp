#include "video_refresh_timer.hpp"
#include <iostream>
#include <chrono>
#include <algorithm>

VideoRefreshTimer::VideoRefreshTimer(PlayerState* state, int interval_ms)
    : state_(state), interval_ms_(interval_ms), running_(false)
{
}

VideoRefreshTimer::~VideoRefreshTimer()
{
    stop();
    join();
}

void VideoRefreshTimer::start() 
{
    if (running_) return;
    
    running_ = true;
    state_->thread_started();
    thread_ = std::thread(&VideoRefreshTimer::run, this);
}

void VideoRefreshTimer::join() 
{
    if (thread_.joinable()) {
        thread_.join();
    }
}

void VideoRefreshTimer::stop() 
{ 
    running_ = false; 
}

void VideoRefreshTimer::setInterval(int interval_ms) 
{ 
    interval_ms_ = interval_ms; 
}

int VideoRefreshTimer::getInterval() const 
{ 
    return interval_ms_; 
}

void VideoRefreshTimer::run() 
{
    std::cout << "VideoRefreshTimer: Starting with interval " << interval_ms_ << "ms" << std::endl;
    
    int consecutive_fast_frames = 0;
    int consecutive_slow_frames = 0;
    
    while (running_ && !state_->quit) 
    {
        // 基于音频时钟同步视频
        double audio_clock = state_->get_master_clock();
        double video_clock = state_->video_clock.load();
        
        // 计算音视频时钟差异
        double diff = audio_clock - video_clock;
        
        // 动态调整刷新间隔
        if (diff > AV_NOSYNC_THRESHOLD) {
            // 视频落后太多，加快显示
            consecutive_fast_frames++;
            consecutive_slow_frames = 0;
            
            if (consecutive_fast_frames > 5) {
                interval_ms_ = std::max(5, interval_ms_ - 2);
                consecutive_fast_frames = 0;
            }
        } else if (diff < -AV_NOSYNC_THRESHOLD) {
            // 视频超前，减慢显示
            consecutive_slow_frames++;
            consecutive_fast_frames = 0;
            
            if (consecutive_slow_frames > 5) {
                interval_ms_ = std::min(100, interval_ms_ + 2);
                consecutive_slow_frames = 0;
            }
        } else {
            // 同步良好，重置计数器
            consecutive_fast_frames = 0;
            consecutive_slow_frames = 0;
        }
        
        // 发送刷新事件
        SDL_Event event;
        event.type = FF_REFRESH_EVENT;
        SDL_PushEvent(&event);
        
        // 等待下一次刷新
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
    }
    
    std::cout << "VideoRefreshTimer: Exiting" << std::endl;
    state_->thread_finished();
}