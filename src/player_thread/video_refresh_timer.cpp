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
    if (thread_.joinable()) 
    {
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
    
    const double AV_SYNC_THRESHOLD = 0.01;
    const double AV_NOSYNC_THRESHOLD = 10.0;
    
    double frame_timer = (double)av_gettime() / 1000000.0;
    double frame_last_delay = 0.04; // 初始假设40ms帧延迟
    double frame_last_pts = 0;
    
    while (running_ && !state_->quit) 
    {
        // 获取音频和视频时钟
        double audio_clock = state_->get_master_clock();
        double video_clock = state_->video_clock.get();
        
        // 计算音视频时钟差异
        double diff = video_clock - audio_clock;
        
        // 动态调整刷新间隔
        double delay = interval_ms_ / 1000.0; // 转换为秒
        
        if (fabs(diff) < AV_NOSYNC_THRESHOLD) 
        {
            // 同步阈值
            double sync_threshold = (delay > AV_SYNC_THRESHOLD) ? delay : AV_SYNC_THRESHOLD;
            
            if (fabs(diff) > sync_threshold) 
            {
                if (diff > 0) 
                {
                    // 视频超前，增加延迟
                    delay = delay * 2;
                } 
                else 
                {
                    // 视频落后，减少延迟
                    delay = delay / 2;
                }
            }
        }
        
        // 限制延迟范围
        delay = std::max(0.01, std::min(0.1, delay));
        
        // 发送刷新事件
        SDL_Event event;
        event.type = FF_REFRESH_EVENT;
        SDL_PushEvent(&event);
        
        // 计算实际需要等待的时间
        double current_time = (double)av_gettime() / 1000000.0;
        double actual_delay = delay - (current_time - frame_timer);
        
        if (actual_delay < 0.01) 
        {
            actual_delay = 0.01;
        }
        
        // 等待下一次刷新
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(actual_delay * 1000)));
        
        // 更新帧计时器
        frame_timer = current_time + actual_delay;
    }
    
    std::cout << "VideoRefreshTimer: Exiting" << std::endl;
    state_->thread_finished();
}