#include "decode.hpp"
#include <iostream>

Decode::~Decode()
{
    close();
}

bool Decode::open(AVCodecParameters* codecpar) 
{
    close();
    
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        std::cerr << "Failed to find decoder for codec: " << codecpar->codec_id << std::endl;
        return false;
    }
    
    codec_ctx_ = avcodec_alloc_context3(codec);
    if (!codec_ctx_) {
        std::cerr << "Failed to allocate codec context" << std::endl;
        return false;
    }
    
    if (avcodec_parameters_to_context(codec_ctx_, codecpar) < 0) {
        std::cerr << "Failed to copy codec parameters to context" << std::endl;
        close();
        return false;
    }
    
    // 设置线程数（可选）
    codec_ctx_->thread_count = 0; // 自动选择线程数
    
    if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
        std::cerr << "Failed to open codec" << std::endl;
        close();
        return false;
    }
    
    is_external_ctx_ = false; // 标记为内部创建的上下文
    std::cout << "Decoder opened: " << avcodec_get_name(codecpar->codec_id) << std::endl;
    return true;
}

bool Decode::sendPacket(const AVPacket* pkt) 
{
    if (!codec_ctx_ || !pkt) return false;
    int ret = avcodec_send_packet(codec_ctx_, pkt);
    if (ret == AVERROR(EAGAIN)) {
        return false; // 解码器需要先接收帧
    }
    return ret == 0;
}

bool Decode::receiveFrame(AVFrame* frame) 
{
    if (!codec_ctx_ || !frame) return false;
    int ret = avcodec_receive_frame(codec_ctx_, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return false;
    }
    return ret == 0;
}

void Decode::close() 
{
    if (codec_ctx_ && !is_external_ctx_) {
        avcodec_free_context(&codec_ctx_);
    }
    codec_ctx_ = nullptr;
    is_external_ctx_ = false;
}

void Decode::flush() 
{
    if (codec_ctx_) {
        avcodec_flush_buffers(codec_ctx_);
    }
}