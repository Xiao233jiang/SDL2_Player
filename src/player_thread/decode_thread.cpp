#include "decode_thread.hpp"
#include "../player_core/decode/audio_decode.hpp"
#include "../player_core/decode/video_decode.hpp"
#include "../player_core/utils/timestamp_utils.hpp"
#include <thread>
#include <iostream>
#include <cmath> // 需要包含cmath以使用std::isnan

// 模板类实现
template<typename Decoder, typename PacketQueue, typename FrameQueue>
DecodeThread<Decoder, PacketQueue, FrameQueue>::DecodeThread(
    Decoder* decoder, PacketQueue* pkt_queue, FrameQueue* frame_queue, 
    PlayerState* state, const std::string& name)
    : decoder_(decoder), pkt_queue_(pkt_queue), frame_queue_(frame_queue), 
      state_(state), name_(name), running_(false)
{
}

template<typename Decoder, typename PacketQueue, typename FrameQueue>
DecodeThread<Decoder, PacketQueue, FrameQueue>::~DecodeThread()
{
    stop();
    join();
}

template<typename Decoder, typename PacketQueue, typename FrameQueue>
void DecodeThread<Decoder, PacketQueue, FrameQueue>::start() 
{
    running_ = true;
    state_->thread_started();
    thread_ = std::thread(&DecodeThread::run, this);
}

template<typename Decoder, typename PacketQueue, typename FrameQueue>
void DecodeThread<Decoder, PacketQueue, FrameQueue>::join() 
{
    if (thread_.joinable()) {
        thread_.join();
    }
}

template<typename Decoder, typename PacketQueue, typename FrameQueue>
void DecodeThread<Decoder, PacketQueue, FrameQueue>::stop() 
{ 
    running_ = false; 
}

template<typename Decoder, typename PacketQueue, typename FrameQueue>
void DecodeThread<Decoder, PacketQueue, FrameQueue>::run() 
{
    std::cout << name_ << ": Starting..." << std::endl;

    AVPacket pkt;
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        std::cerr << name_ << ": Failed to allocate frame" << std::endl;
        state_->thread_finished();
        return;
    }

    bool is_audio = (name_.find("Audio") != std::string::npos);
    int stream_index = is_audio ? state_->audio_stream : state_->video_stream;
    
    if (stream_index < 0) {
        std::cout << name_ << ": No stream available, exiting" << std::endl;
        av_frame_free(&frame);
        state_->thread_finished();
        return;
    }

    AVStream* stream = state_->fmt_ctx->streams[stream_index];
    int64_t frame_number = 0;
    int frame_count = 0;
    
    // 精准 seek 相关变量
    bool seeking_flag = false;
    double target_seek_time = 0.0;

    while (running_ && !state_->quit) 
    {
        // 检查是否应该退出
        if ((is_audio && state_->audio_eof && pkt_queue_->empty()) ||
            (!is_audio && state_->video_eof && pkt_queue_->empty())) 
        {
            std::cout << name_ << ": EOF reached and queue empty, exiting" << std::endl;
            break;
        }

        // 获取数据包
        if (!pkt_queue_->pop(pkt, state_->quit, 100)) {
            if (state_->quit) break;
            continue;
        }

        // ✅ 修复：正确检查 flush 包
        if (pkt.stream_index == FF_FLUSH_PACKET_STREAM_INDEX) {
            printf("%s: Received flush packet\n", name_.c_str());
            
            // 刷新解码器缓冲区
            decoder_->flush();
            
            // 清空帧队列
            int cleared_frames = frame_queue_->size();
            frame_queue_->clear();
            printf("%s: Decoder flushed, cleared %d frames\n", name_.c_str(), cleared_frames);
            
            // 设置精准 seek 状态
            if (pkt.pos != AV_NOPTS_VALUE) {
                seeking_flag = true;
                target_seek_time = pkt.pos / (double)AV_TIME_BASE;
                printf("%s: Starting accurate seek to %.2fs\n", name_.c_str(), target_seek_time);
            }
            
            // ✅ 重要：对于视频解码线程，完成 flush 后重置 seeking 状态
            if (!is_audio) {
                printf("%s: Resetting seeking state\n", name_.c_str());
                state_->seeking.store(false);
            }
            
            av_packet_unref(&pkt);
            continue;
        }

        // ✅ 修复：检查 EOF 包
        if (pkt.data == nullptr && pkt.size == 0) {
            printf("%s: EOF packet received\n", name_.c_str());
            av_packet_unref(&pkt);
            break;
        }

        // 只处理本线程对应的流
        if (pkt.stream_index != stream_index) {
            printf("%s: Ignoring packet from stream %d (expected %d)\n", 
                   name_.c_str(), pkt.stream_index, stream_index);
            av_packet_unref(&pkt);
            continue;
        }

        // 发送到解码器
        if (!decoder_->sendPacket(&pkt)) {
            std::cerr << name_ << ": Error sending packet to decoder" << std::endl;
            av_packet_unref(&pkt);
            continue;
        }

        // 接收所有可用的解码帧
        while (running_ && !state_->quit) 
        {
            if (!decoder_->receiveFrame(frame)) {
                break; // 需要更多数据或EOF
            }

            frame_count++;
            
            // 处理时间戳
            if (frame->pts == AV_NOPTS_VALUE) 
            {
                if (pkt.pts != AV_NOPTS_VALUE) 
                {
                    frame->pts = av_rescale_q(pkt.pts, stream->time_base, 
                                            decoder_->getCodecCtx()->time_base);
                } 
                else if (pkt.dts != AV_NOPTS_VALUE) 
                {
                    frame->pts = av_rescale_q(pkt.dts, stream->time_base, 
                                            decoder_->getCodecCtx()->time_base);
                } 
                else 
                {
                    frame->pts = frame_number;
                    if (is_audio) {
                        frame_number += frame->nb_samples;
                    } else {
                        frame_number++;
                    }
                }
            }
            
            // ✅ 改进：精准 seek 处理
            if (seeking_flag) {
                double frame_time_seconds = 0.0;
                
                if (is_audio) {
                    frame_time_seconds = frame->pts * av_q2d(decoder_->getCodecCtx()->time_base);
                } else {
                    frame_time_seconds = frame->pts * av_q2d(stream->time_base);
                }
                
                // 检查是否到达目标位置
                double time_diff = frame_time_seconds - target_seek_time;
                
                if (time_diff < -0.5) { // 如果帧时间比目标时间早0.5秒以上，丢弃
                    printf("%s: Dropping frame at %.2fs (target: %.2fs, diff: %.2fs)\n", 
                           name_.c_str(), frame_time_seconds, target_seek_time, time_diff);
                    av_frame_unref(frame);
                    continue;
                } else {
                    // 到达目标位置附近，停止 seeking
                    seeking_flag = false;
                    printf("%s: Seek completed at %.2fs (target: %.2fs, diff: %.2fs)\n", 
                           name_.c_str(), frame_time_seconds, target_seek_time, time_diff);
                }
            }
            
            // 对于视频帧，保存PTS到opaque字段
            if (!is_audio) {
                double* pts_ptr = (double*)av_malloc(sizeof(double));
                *pts_ptr = (frame->pts == AV_NOPTS_VALUE) ? NAN : 
                        frame->pts * av_q2d(stream->time_base);
                frame->opaque = pts_ptr;
            }

            // 克隆帧并放入队列
            AVFrame* cloned_frame = av_frame_clone(frame);
            if (cloned_frame) 
            {
                if (!frame_queue_->push(cloned_frame, true, 100)) 
                {
                    // 队列满了，丢弃帧
                    av_frame_free(&cloned_frame);
                } 
                else 
                {
                    // 更新统计信息
                    if (is_audio) {
                        state_->stats.audio_frames++;
                    } else {
                        state_->stats.video_frames++;
                    }
                }
            }
            
            av_frame_unref(frame);
        }
        
        av_packet_unref(&pkt);
    }

    printf("%s: Finished after decoding %d frames\n", name_.c_str(), frame_count);
    av_frame_free(&frame);
    state_->thread_finished();
}

// 显式实例化
template class DecodeThread<AudioDecode, SafeQueue<AVPacket>, SafeQueue<AVFrame*>>;
template class DecodeThread<VideoDecode, SafeQueue<AVPacket>, SafeQueue<AVFrame*>>;