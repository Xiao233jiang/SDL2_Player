#include "demux_thread.hpp"
#include <iostream>
#include <thread>
#include <climits> 
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
        // ✅ 修复：优先处理 seek 请求
        if (state_->seek_request.load()) {
            printf("DemuxThread: Detected seek request in main loop\n");
            if (handleSeekRequest()) {
                printf("DemuxThread: Seek handled successfully, continuing...\n");
                continue; // seek 成功后继续读取
            } else {
                printf("DemuxThread: Seek handling failed\n");
            }
        }

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
                
                // 发送EOF包到队列
                AVPacket eof_pkt;
                av_init_packet(&eof_pkt);
                eof_pkt.data = nullptr;
                eof_pkt.size = 0;
                
                if (state_->audio_stream >= 0) 
                {
                    eof_pkt.stream_index = state_->audio_stream; // 使用正常的stream_index
                    state_->audio_packet_queue.push(eof_pkt, true, 100);
                    state_->audio_eof = true;
                }
                
                if (state_->video_stream >= 0) 
                {
                    eof_pkt.stream_index = state_->video_stream; // 使用正常的stream_index
                    state_->video_packet_queue.push(eof_pkt, true, 100);
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

bool DemuxThread::handleSeekRequest()
{
    if (!state_->seek_request.load()) {
        return false;
    }
    
    printf("=== DemuxThread::handleSeekRequest START ===\n");
    
    // 获取 seek 参数
    int64_t seek_pos = state_->seek_pos.load();
    int64_t seek_rel = state_->seek_rel.load();
    int seek_flags = state_->seek_flags.load();
    
    printf("Seek parameters:\n");
    printf("  Target position: %lld (%.2fs)\n", seek_pos, seek_pos / (double)AV_TIME_BASE);
    printf("  Relative: %lld (%.2fs)\n", seek_rel, seek_rel / (double)AV_TIME_BASE);
    printf("  Flags: %d\n", seek_flags);
    
    // 设置seeking状态
    state_->seeking.store(true);
    
    // ✅ 修复：使用更好的seek方法
    int ret = av_seek_frame(state_->fmt_ctx, -1, seek_pos, seek_flags);
    
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        printf("ERROR: av_seek_frame failed: %s (code: %d)\n", errbuf, ret);
        
        // 重置seek状态
        state_->seek_request.store(false);
        state_->seeking.store(false);
        return false;
    }
    
    printf("SUCCESS: av_seek_frame completed\n");
    
    // 清空所有队列
    printf("Clearing all queues...\n");
    
    int audio_packets_cleared = state_->audio_packet_queue.size();
    int video_packets_cleared = state_->video_packet_queue.size();
    int audio_frames_cleared = state_->audio_frame_queue.size();
    int video_frames_cleared = state_->video_frame_queue.size();
    
    state_->audio_packet_queue.clear();
    state_->video_packet_queue.clear();
    state_->audio_frame_queue.clear();
    state_->video_frame_queue.clear();
    
    printf("Cleared queues: AP=%d, VP=%d, AF=%d, VF=%d\n", 
           audio_packets_cleared, video_packets_cleared, 
           audio_frames_cleared, video_frames_cleared);
    
    // ✅ 发送 flush 包到解码器
    printf("Sending flush packets...\n");
    
    if (state_->audio_stream >= 0) {
        AVPacket flush_pkt;
        av_init_packet(&flush_pkt);
        flush_pkt.data = nullptr;
        flush_pkt.size = 0;
        flush_pkt.stream_index = FF_FLUSH_PACKET_STREAM_INDEX;
        flush_pkt.pos = seek_pos;
        
        if (state_->audio_packet_queue.push(flush_pkt, true, 1000)) {
            printf("  Audio flush packet sent\n");
        } else {
            printf("  ERROR: Failed to send audio flush packet\n");
        }
    }
    
    if (state_->video_stream >= 0) {
        AVPacket flush_pkt;
        av_init_packet(&flush_pkt);
        flush_pkt.data = nullptr;
        flush_pkt.size = 0;
        flush_pkt.stream_index = FF_FLUSH_PACKET_STREAM_INDEX;
        flush_pkt.pos = seek_pos;
        
        if (state_->video_packet_queue.push(flush_pkt, true, 1000)) {
            printf("  Video flush packet sent\n");
        } else {
            printf("  ERROR: Failed to send video flush packet\n");
        }
    }
    
    // 重置 EOF 标志
    state_->audio_eof.store(false);
    state_->video_eof.store(false);
    state_->demux_finished.store(false);
    
    // 更新时钟到目标位置
    double seek_time = seek_pos / (double)AV_TIME_BASE;
    state_->audio_clock.set(seek_time);
    state_->video_clock.set(seek_time);
    printf("Updated clocks to %.2fs\n", seek_time);
    
    // 重置 seek 请求标志
    state_->seek_request.store(false);
    
    printf("=== DemuxThread::handleSeekRequest COMPLETED ===\n");
    return true;
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