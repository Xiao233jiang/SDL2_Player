#include "demux_thread.hpp"
#include <iostream>
#include <thread>
#include "thread_utils.hpp"

void DemuxThread::run() 
{
    THREAD_SAFE_COUT("DemuxThread: Starting...");
    
    // 打开输入文件
    if (avformat_open_input(&state_->fmt_ctx, state_->filename.c_str(), nullptr, nullptr) < 0) 
    {
        state_->set_error(PlayerError::FILE_OPEN_FAILED, "Cannot open file: " + state_->filename);
        // 通知主线程，避免死等
        state_->demux_ready_cv.notify_one();
        return;
    }
    
    // 获取流信息
    if (avformat_find_stream_info(state_->fmt_ctx, nullptr) < 0) {
        state_->set_error(PlayerError::STREAM_INFO_FAILED, "Could not find stream information");
        // 通知主线程
        state_->demux_ready_cv.notify_one();
        return;
    }
    
    // 查找音频和视频流
    state_->audio_stream = -1;
    state_->video_stream = -1;
    
    for (unsigned int i = 0; i < state_->fmt_ctx->nb_streams; i++) 
    {
        if (state_->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && 
            state_->audio_stream < 0) 
        {
            state_->audio_stream = i;
        }
        else if (state_->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && 
                 state_->video_stream < 0) 
        {
            state_->video_stream = i;
        }
    }
    
    if (state_->audio_stream < 0 && state_->video_stream < 0) {
        state_->set_error(PlayerError::STREAM_INFO_FAILED, "No audio or video streams found");
        // 通知主线程
        state_->demux_ready_cv.notify_one();
        return;
    }
    
    THREAD_SAFE_COUT("DemuxThread: Found audio stream: " << state_->audio_stream 
                  << ", video stream: " << state_->video_stream);
    
    // 通知主线程准备完成
    {
        std::lock_guard<std::mutex> lock(state_->demux_ready_mutex);
        state_->demux_ready = true;
    }
    state_->demux_ready_cv.notify_one();
    
    AVPacket pkt;
    int packet_count = 0;
    
    // 主循环：读取数据包并放入相应队列
    while (running_ && !state_->quit) 
    {
        // 检查队列是否已满
        if ((state_->audio_stream >= 0 && state_->audio_packet_queue.size() >= MAX_AUDIO_PACKETS) ||
            (state_->video_stream >= 0 && state_->video_packet_queue.size() >= MAX_VIDEO_PACKETS)) 
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // 读取数据包
        int ret = av_read_frame(state_->fmt_ctx, &pkt);
        if (ret < 0) 
        {
            if (ret == AVERROR_EOF) 
            {
                THREAD_SAFE_COUT("DemuxThread: End of file reached");
                state_->demux_finished = true;
                
                // 发送空包到队列以刷新解码器
                AVPacket flush_pkt;
                av_init_packet(&flush_pkt);
                flush_pkt.data = nullptr;
                flush_pkt.size = 0;
                
                if (state_->audio_stream >= 0) 
                {
                    flush_pkt.stream_index = state_->audio_stream;
                    state_->audio_packet_queue.push(flush_pkt, true, 100);
                    state_->audio_eof = true;
                }
                
                if (state_->video_stream >= 0) 
                {
                    flush_pkt.stream_index = state_->video_stream;
                    state_->video_packet_queue.push(flush_pkt, true, 100);
                    state_->video_eof = true;
                }
                
                break;
            }
            
            // 非EOF错误，继续尝试
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        packet_count++;
        if (packet_count % 100 == 0) 
        {
            THREAD_SAFE_COUT("DemuxThread: Read packet " << packet_count 
                          << ", stream: " << pkt.stream_index);
        }
        
        // 克隆数据包并放入相应队列
        AVPacket* cloned_pkt = av_packet_alloc();
        if (av_packet_ref(cloned_pkt, &pkt) == 0) 
        {
            if (cloned_pkt->stream_index == state_->audio_stream) 
            {
                if (!state_->audio_packet_queue.push(*cloned_pkt, true, 100)) 
                {
                    av_packet_free(&cloned_pkt);
                }
                else 
                {
                    state_->stats.audio_packets++;
                }
            } 
            else if (cloned_pkt->stream_index == state_->video_stream) 
            {
                if (!state_->video_packet_queue.push(*cloned_pkt, true, 100)) 
                {
                    av_packet_free(&cloned_pkt);
                }
                else 
                {
                    state_->stats.video_packets++;
                }
            } 
            else 
            {
                av_packet_free(&cloned_pkt);
            }
        }
        
        av_packet_unref(&pkt);
    }
    
    THREAD_SAFE_COUT("DemuxThread: Finished after reading " << packet_count << " packets");
    state_->thread_finished();
}

void DemuxThread::start() 
{
    running_ = true;
    state_->thread_started();
    thread_ = std::thread(&DemuxThread::run, this);
}

void DemuxThread::stop() 
{ 
    running_ = false; 
}

void DemuxThread::join() 
{
    if (thread_.joinable()) 
        thread_.join();
}