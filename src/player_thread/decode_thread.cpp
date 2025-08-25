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

    bool is_audio = (name_.find("audio") != std::string::npos);
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

        // 检查是否是刷新包
        if (pkt.data == nullptr) {
            decoder_->flush();
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
                    // 生成基于帧数的时间戳
                    frame->pts = frame_number;
                    if (is_audio) 
                    {
                        frame_number += frame->nb_samples;
                    } 
                    else 
                    {
                        frame_number++;
                    }
                }
            }
            
            // 特别处理音频帧的时间戳
            if (is_audio) 
            {
                // 计算时间戳（以秒为单位）
                double pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : 
                            frame->pts * av_q2d(decoder_->getCodecCtx()->time_base);
                
                // 对于音频帧，我们需要确保时间戳是有效的
                if (std::isnan(pts)) 
                {
                    // 如果没有有效时间戳，使用基于样本数的估计
                    pts = frame_number * av_q2d(decoder_->getCodecCtx()->time_base);
                }
                
                // 更新时间戳（转换为解码器时间基）
                frame->pts = static_cast<int64_t>(pts / av_q2d(decoder_->getCodecCtx()->time_base));
                
                // 更新帧计数器
                frame_number += frame->nb_samples;
            }
            else 
            {
                // 对于视频帧，保存PTS到opaque字段
                double* pts_ptr = (double*)av_malloc(sizeof(double));
                AVStream* stream = state_->fmt_ctx->streams[state_->video_stream];
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
                    std::cout << name_ << ": Frame queue full, dropping frame" << std::endl;
                    av_frame_free(&cloned_frame);
                } 
                else 
                {
                    // 更新统计信息
                    if (is_audio) 
                    {
                        state_->stats.audio_frames++;
                    } 
                    else 
                    {
                        state_->stats.video_frames++;
                    }
                }
            }
            
            av_frame_unref(frame);
        }
        
        av_packet_unref(&pkt);
    }

    std::cout << name_ << ": Finished after decoding " << frame_count << " frames" << std::endl;
    av_frame_free(&frame);
    state_->thread_finished();
}

// 显式实例化
template class DecodeThread<AudioDecode, SafeQueue<AVPacket>, SafeQueue<AVFrame*>>;
template class DecodeThread<VideoDecode, SafeQueue<AVPacket>, SafeQueue<AVFrame*>>;