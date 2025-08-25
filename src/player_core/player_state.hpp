#pragma once

#include <string>
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <SDL2/SDL.h>
#include "utils/safe_queue.hpp"
#include "utils/player_constants.hpp"
#include "../play/clock.hpp"
#include "../ffmpeg_utils/ffmpeg_headers.hpp"

/**
 * 播放器全局状态，管理解封装、解码、队列、SDL 资源等。
 */
class PlayerState {
public:
    // 基本状态
    std::string filename;
    std::atomic<bool> quit{false};
    std::atomic<PlayerError> error{PlayerError::NONE};
    std::string error_message;

    // 解封装状态
    std::atomic<bool> demux_ready{false};
    std::atomic<bool> demux_finished{false};
    std::condition_variable demux_ready_cv;
    std::mutex demux_ready_mutex;

    // 流结束标志
    std::atomic<bool> audio_eof{false};
    std::atomic<bool> video_eof{false};

    // 解封装器
    AVFormatContext* fmt_ctx = nullptr;

    // 音视频流索引
    int audio_stream = -1;
    int video_stream = -1;

    // 解码器上下文
    AVCodecContext* audio_ctx = nullptr;
    AVCodecContext* video_ctx = nullptr;

    // 队列（带容量限制）
    SafeQueue<AVPacket> audio_packet_queue{MAX_AUDIO_PACKETS, [](AVPacket& pkt) {
        av_packet_unref(&pkt);
    }};
    
    SafeQueue<AVPacket> video_packet_queue{MAX_VIDEO_PACKETS, [](AVPacket& pkt) {
        av_packet_unref(&pkt);
    }};
    
    SafeQueue<AVFrame*> audio_frame_queue{MAX_AUDIO_FRAMES, [](AVFrame*& frame) {
        if (frame) av_frame_free(&frame);
    }};
    
    SafeQueue<AVFrame*> video_frame_queue{MAX_VIDEO_FRAMES, [](AVFrame*& frame) {
        if (frame) av_frame_free(&frame);
    }};

    // SDL 相关
    SDL_Texture* texture = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Window* window = nullptr;

    // 图像转换
    SwsContext* sws_ctx = nullptr;

    // 音频缓冲区
    uint8_t* audio_buf = nullptr;
    unsigned int audio_buf_size = 0;
    unsigned int audio_buf_index = 0;

    // 统计信息
    struct Stats {
        std::atomic<int64_t> audio_packets{0};
        std::atomic<int64_t> video_packets{0};
        std::atomic<int64_t> audio_frames{0};
        std::atomic<int64_t> video_frames{0};
        std::atomic<int64_t> audio_bytes{0};
        std::atomic<int64_t> video_bytes{0};
        
        // 添加重置方法
        void reset() {
            audio_packets.store(0);
            video_packets.store(0);
            audio_frames.store(0);
            video_frames.store(0);
            audio_bytes.store(0);
            video_bytes.store(0);
        }
    } stats;

    // 时钟管理
    Clock audio_clock;
    Clock video_clock;

    // 时间同步
    std::mutex clock_mutex;

    // 线程管理
    std::atomic<int> running_threads{0};
    std::condition_variable threads_cv;
    std::mutex threads_mutex;

    // 调试限制
    long maxFramesToDecode = 0;
    int currentFrameIndex = 0;

    PlayerState();
    ~PlayerState();
    
    void clear();
    void set_error(PlayerError err, const std::string& msg = "");
    
    bool wait_for_threads(int timeout_ms = 5000);
    void thread_started();
    void thread_finished();
    
    void update_audio_clock(double pts, int data_size = 0);
    void update_video_clock(double pts);
    double get_master_clock();
    
    // 队列状态检查
    bool audio_queue_full() const { return audio_packet_queue.size() >= MAX_AUDIO_PACKETS; }
    bool video_queue_full() const { return video_packet_queue.size() >= MAX_VIDEO_PACKETS; }
    bool audio_frame_queue_full() const { return audio_frame_queue.size() >= MAX_AUDIO_FRAMES; }
    bool video_frame_queue_full() const { return video_frame_queue.size() >= MAX_VIDEO_FRAMES; }
};