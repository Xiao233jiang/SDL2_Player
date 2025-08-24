#pragma once
#include <thread>
#include <atomic>
#include <iostream>
#include <string>
#include "../player_core/player_state.hpp"
#include "../player_core/utils/safe_queue.hpp"

// 前向声明
class AudioDecode;
class VideoDecode;

template<typename Decoder, typename PacketQueue, typename FrameQueue>
class DecodeThread {
public:
    DecodeThread(Decoder* decoder, PacketQueue* pkt_queue, FrameQueue* frame_queue, 
                 PlayerState* state, const std::string& name = "DecodeThread");

    ~DecodeThread();

    void start();
    void join();
    void stop();

    const std::string& name() const { return name_; }

private:
    void run();

    Decoder* decoder_;
    PacketQueue* pkt_queue_;
    FrameQueue* frame_queue_;
    PlayerState* state_;
    std::string name_;
    std::thread thread_;
    std::atomic<bool> running_;
};

// 显式实例化声明
extern template class DecodeThread<AudioDecode, SafeQueue<AVPacket>, SafeQueue<AVFrame*>>;
extern template class DecodeThread<VideoDecode, SafeQueue<AVPacket>, SafeQueue<AVFrame*>>;

// 添加类型别名
using AudioDecodeThread = DecodeThread<AudioDecode, SafeQueue<AVPacket>, SafeQueue<AVFrame*>>;
using VideoDecodeThread = DecodeThread<VideoDecode, SafeQueue<AVPacket>, SafeQueue<AVFrame*>>;