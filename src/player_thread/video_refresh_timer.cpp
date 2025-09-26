#include "video_refresh_timer.hpp"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cmath>

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
    
    while (running_ && !state_->quit) 
    {
        // 检查是否有视频流
        if (state_->video_stream < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // 检查视频帧队列是否有数据
        if (state_->video_frame_queue.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // 计算下一帧的延迟时间
        int delay_ms = calculateFrameDelay();
        
        // 发送刷新事件
        SDL_Event event;
        event.type = FF_REFRESH_EVENT;
        if (SDL_PushEvent(&event) < 0) {
            std::cerr << "VideoRefreshTimer: Failed to push refresh event" << std::endl;
        }
        
        // 根据计算的延迟等待
        if (delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        } else {
            // 如果延迟为负数或0，立即处理下一帧
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    std::cout << "VideoRefreshTimer: Exiting" << std::endl;
    state_->thread_finished();
}

int VideoRefreshTimer::calculateFrameDelay()
{
    // 如果队列为空，使用默认延迟
    if (state_->video_frame_queue.empty()) {
        return interval_ms_;
    }
    
    // 查看队列中的下一帧，但不取出
    AVFrame* next_frame = nullptr;
    if (!state_->video_frame_queue.front(next_frame) || !next_frame) {
        return interval_ms_;
    }
    
    // 计算视频PTS
    double video_pts = 0.0;
    if (next_frame->opaque) {
        video_pts = *((double*)next_frame->opaque);
    } else {
        return interval_ms_; // 没有时间戳信息，使用默认延迟
    }
    
    // 获取当前音频时钟
    double audio_clock = state_->get_master_clock();
    
    // 计算同步延迟
    double diff = video_pts - audio_clock;
    
    // 计算基础帧延迟（根据帧率）
    double frame_delay = interval_ms_ / 1000.0; // 转换为秒
    if (state_->video_ctx && state_->video_ctx->framerate.num > 0) {
        frame_delay = av_q2d(av_inv_q(state_->video_ctx->framerate));
    }
    
    // 根据同步差异调整延迟
    double sync_threshold = frame_delay; // 同步阈值
    
    if (diff <= -sync_threshold) {
        // 视频落后太多，跳过这一帧（返回0延迟）
        return 0;
    } else if (diff >= sync_threshold) {
        // 视频超前，增加延迟
        frame_delay += diff;
    }
    
    // 限制延迟范围（避免极端值）
    frame_delay = std::max(0.01, std::min(0.1, frame_delay)); // 10ms到100ms
    
    return static_cast<int>(frame_delay * 1000); // 转换为毫秒
}