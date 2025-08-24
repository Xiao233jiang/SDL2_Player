#pragma once

#include "../ffmpeg_utils/ffmpeg_headers.hpp"

class Demux
{
public:
    Demux();
    ~Demux();

    // 打开文件，返回 AVFormatContext*
    bool open(const char* filename);

    // 查找第一个视频流索引
    int findFirstVideoStream() const;

    // 查找第一个音频流索引
    int findFirstAudioStream() const;

    // 读取一个包
    bool readPacket(AVPacket* pkt);

    AVFormatContext* releaseFormatCtx();

    // 获取上下文
    AVFormatContext* getFormatCtx() const { return fmt_ctx_; }

    void close();

private:
    AVFormatContext * fmt_ctx_ = nullptr;
};