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
    // 设置退出标志
    quit.store(true);

    // 先停止所有音频相关操作
    if (audio_ctx) {
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
    if (audio_ctx) {
        avcodec_free_context(&audio_ctx);
        audio_ctx = nullptr;
    }
    
    if (video_ctx) {
        avcodec_free_context(&video_ctx);
        video_ctx = nullptr;
    }
    
    // 清理格式上下文
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
        fmt_ctx = nullptr;
    }
    
    // 清理图像转换上下文
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = nullptr;
    }
    
    // 清理音频缓冲区
    if (audio_buf) {
        av_free(audio_buf);
        audio_buf = nullptr;
        audio_buf_size = 0;
        audio_buf_index = 0;
    }
    
    // 清空所有队列 - 让队列自己的清理函数处理内存
    audio_packet_queue.clear();
    video_packet_queue.clear();
    audio_frame_queue.clear();  // 使用队列自己的清理函数
    video_frame_queue.clear();  // 使用队列自己的清理函数
    
    // 重置所有状态
    demux_ready.store(false);
    demux_finished.store(false);
    audio_eof.store(false);
    video_eof.store(false);
    audio_stream = -1;
    video_stream = -1;
    
    // 重置统计信息
    stats.reset();
    
    // 重置时钟 - 使用 Clock 类的 set 方法
    audio_clock.set(0);
    video_clock.set(0);
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