#pragma once 

#include "../ffmpeg_utils/ffmpeg_headers.hpp"
#include <memory>

class Decode 
{
public:
    Decode() = default;
    virtual ~Decode();

    // 打开解码器
    virtual bool open(AVCodecParameters* codecpar);

    // 发送压缩包到解码器
    virtual bool sendPacket(const AVPacket* pkt);

    // 从解码器获取解码帧
    virtual bool receiveFrame(AVFrame* frame);

    // 刷新解码器的方法
    void flush();

    // 关闭解码器
    virtual void close();

    AVCodecContext* getCodecCtx() const { return codec_ctx_; }

protected:
    AVCodecContext* codec_ctx_ = nullptr;
    bool is_external_ctx_ = false; // 标记上下文是否由外部传入
};