#include "player_state.hpp"
#include <iostream>
#include <thread>

extern "C" {
#include <libavutil/frame.h>
#include <libavcodec/packet.h>
#include <libavutil/time.h>
}

PlayerState::PlayerState() 
{
    // 初始化队列退出标志
    audio_packet_queue.set_quit(false);
    video_packet_queue.set_quit(false);
    audio_frame_queue.set_quit(false);
    video_frame_queue.set_quit(false);
}

PlayerState::~PlayerState() 
{
    clear();
}

void PlayerState::clear() 
{
    clearForReload(true); // 完全清理，包括设置quit
}

void PlayerState::clearForReload(bool set_quit_flag) 
{
    // 只在完全清理时设置退出标志
    if (set_quit_flag) {
        quit.store(true);
    }

    // 先停止所有音频相关操作
    if (audio_ctx) 
    {
        // 等待音频处理完成
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 通知所有等待的线程
    demux_ready_cv.notify_all();
    threads_cv.notify_all();
    
    // 设置队列退出标志
    audio_packet_queue.set_quit(true);
    video_packet_queue.set_quit(true);
    audio_frame_queue.set_quit(true);
    video_frame_queue.set_quit(true);
    
    // 等待所有线程退出
    wait_for_threads(2000);
    
    // 清理解码器上下文
    if (audio_ctx) 
    {
        avcodec_free_context(&audio_ctx);
        audio_ctx = nullptr;
    }
    
    if (video_ctx) 
    {
        avcodec_free_context(&video_ctx);
        video_ctx = nullptr;
    }
    
    // 清理格式上下文
    if (fmt_ctx) 
    {
        avformat_close_input(&fmt_ctx);
        fmt_ctx = nullptr;
    }
    
    // 清理图像转换上下文
    if (sws_ctx) 
    {
        sws_freeContext(sws_ctx);
        sws_ctx = nullptr;
    }
    
    // 清理音频缓冲区
    if (audio_buf) 
    {
        av_free(audio_buf);
        audio_buf = nullptr;
        audio_buf_size = 0;
        audio_buf_index = 0;
    }
    
    // 使用新的reset方法重置队列，而不是clear方法
    audio_packet_queue.reset();  // 修改：使用reset而不是clear
    video_packet_queue.reset();  // 修改：使用reset而不是clear
    audio_frame_queue.reset();   // 修改：使用reset而不是clear
    video_frame_queue.reset();   // 修改：使用reset而不是clear
    
    // 重置所有状态
    demux_ready.store(false);
    demux_finished.store(false);
    audio_eof.store(false);
    video_eof.store(false);
    audio_stream = -1;
    video_stream = -1;
    
    // 重置 seek 相关状态
    seeking.store(false);
    seek_request.store(false);
    seek_pos.store(0);
    seek_rel.store(0);
    seek_flags.store(0);
    seek_target_pts.store(AV_NOPTS_VALUE);
    
    // 重置统计信息
    stats.reset();
    
    // 重置时钟，使用新的reset方法
    audio_clock.reset();  // 修改：使用reset方法
    video_clock.reset();  // 修改：使用reset方法
    
    // 重置错误状态
    error.store(PlayerError::NONE);
    error_message.clear();
    
    // 如果不是完全退出，确保状态可用
    if (!set_quit_flag) {
        quit.store(false); // 重置quit标志，准备加载新文件
        // 队列的reset方法已经重置了quit标志，不需要再次设置
    }
}

void PlayerState::resetForNewFile() 
{
    clearForReload(false); // 清理但不退出
    
    // 重新初始化一些必要的状态
    demux_ready.store(false);
    demux_finished.store(false);
    audio_eof.store(false);
    video_eof.store(false);
    
    // 确保线程计数器重置
    running_threads.store(0);
    
    std::cout << "PlayerState reset for new file" << std::endl;
}

void PlayerState::set_error(PlayerError err, const std::string& msg) 
{
    error.store(err);
    error_message = msg;
    quit.store(true);
    std::cerr << "Player Error: " << msg << std::endl;
}

bool PlayerState::wait_for_threads(int timeout_ms) 
{
    std::unique_lock<std::mutex> lock(threads_mutex);
    return threads_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
        [this]() { return running_threads.load() == 0; });
}

void PlayerState::thread_started() 
{
    std::lock_guard<std::mutex> lock(threads_mutex);
    running_threads.fetch_add(1);
}

void PlayerState::thread_finished() 
{
    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        running_threads.fetch_sub(1);
    }
    threads_cv.notify_all();
}

void PlayerState::update_audio_clock(double pts, int data_size) 
{
    // 不再使用原子变量，而是使用 Clock 类
    audio_clock.set(pts);
    if (data_size > 0) {
        stats.audio_bytes.fetch_add(data_size);
    }
}

void PlayerState::update_video_clock(double pts) 
{
    // 不再使用原子变量，而是使用 Clock 类
    video_clock.set(pts);
}

double PlayerState::get_master_clock() 
{
    // 返回音频时钟的值作为主时钟
    return audio_clock.get();
}

void PlayerState::doSeekAbsolute(double seconds) {
    printf("=== PlayerState::doSeekAbsolute START ===\n");
    printf("Target: %.2f seconds\n", seconds);
    printf("Currently seeking: %s\n", seeking.load() ? "YES" : "NO");
    printf("Has format context: %s\n", fmt_ctx ? "YES" : "NO");
    
    if (!fmt_ctx) {
        printf("ERROR: No format context available\n");
        return;
    }

    // ✅ 修复：允许中断之前的seek操作
    std::lock_guard<std::mutex> lock(seek_mutex);
    
    // 确保时间在有效范围内
    if (seconds < 0.0) seconds = 0.0;
    
    if (fmt_ctx->duration != AV_NOPTS_VALUE) {
        double duration = fmt_ctx->duration / (double)AV_TIME_BASE;
        if (seconds > duration) seconds = duration;
    }
    
    double current_time = get_master_clock();
    double incr = seconds - current_time;
    
    printf("Current time: %.2fs, Target: %.2fs, Increment: %.2fs\n", 
           current_time, seconds, incr);
    
    // 设置 seek 参数
    seek_rel.store((int64_t)(incr * AV_TIME_BASE));
    seek_pos.store((int64_t)(seconds * AV_TIME_BASE));
    seek_flags.store((incr < 0) ? AVSEEK_FLAG_BACKWARD : 0);
    seek_target_pts.store(seek_pos.load());
    
    // ✅ 重要：设置 seek 请求标志
    seek_request.store(true);
    
    printf("Seek request set: pos=%lld, rel=%lld, flags=%d\n",
           seek_pos.load(), seek_rel.load(), seek_flags.load());
    
    // 立即更新时钟以提供视觉反馈
    audio_clock.set(seconds);
    video_clock.set(seconds);
    
    printf("=== PlayerState::doSeekAbsolute END ===\n");
}

void PlayerState::doSeekRelative(double incr_seconds) {
    printf("=== PlayerState::doSeekRelative START ===\n");
    printf("Increment: %.2f seconds\n", incr_seconds);
    
    if (!fmt_ctx) {
        printf("ERROR: No format context available\n");
        return; 
    }

    std::lock_guard<std::mutex> lock(seek_mutex);
    
    // 获取当前播放位置
    double current_time = get_master_clock();
    double target_time = current_time + incr_seconds;
    
    // 确保目标时间在有效范围内
    if (target_time < 0.0) {
        target_time = 0.0;
    }
    
    if (fmt_ctx->duration != AV_NOPTS_VALUE) {
        double duration = fmt_ctx->duration / (double)AV_TIME_BASE;
        if (target_time > duration) {
            target_time = duration;
        }
    }
    
    printf("Current time: %.2fs, Target: %.2fs\n", current_time, target_time);
    
    // 设置 seek 参数
    seek_rel.store((int64_t)(incr_seconds * AV_TIME_BASE));
    seek_pos.store((int64_t)(target_time * AV_TIME_BASE));
    seek_flags.store((incr_seconds < 0) ? AVSEEK_FLAG_BACKWARD : 0);
    seek_target_pts.store(seek_pos.load());
    
    // 重要：设置 seek 请求标志
    seek_request.store(true);
    
    // 预先更新时钟到目标位置
    audio_clock.set(target_time);
    video_clock.set(target_time);
    
    printf("=== PlayerState::doSeekRelative END ===\n");
}