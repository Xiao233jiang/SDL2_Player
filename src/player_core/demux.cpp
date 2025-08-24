#include "demux.hpp"
#include "libavcodec/packet.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include <cstdio>

Demux::Demux()  { }

Demux::~Demux()
{
    close();
}

bool Demux::open(const char* filename)
{
    close();
    // 文件I/O上下文：demuxers读取媒体文件并将其分割为数据块（数据包）。
    if (avformat_open_input(&fmt_ctx_, filename, nullptr, nullptr) < 0)
    {
        printf("Could not open file %s.\n", filename);
        return false;
    }
    // 读取媒体文件的数据包以获取流信息
    if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0)
    {
        printf("Could not find stream information %s.\n", filename);
    }

    // 将有关文件的信息转储到标准错误输出。
    av_dump_format(fmt_ctx_, 0, filename, 0);

    return true;
}

int Demux::findFirstVideoStream() const 
{
    if (!fmt_ctx_) return -1; // 添加空指针检查
    
    for (int i = 0; i < fmt_ctx_->nb_streams; i++)
    {
        if (fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            return i;
    }

    return -1;
}

int Demux::findFirstAudioStream() const 
{
    if (!fmt_ctx_) return -1; // 添加空指针检查
    
    for (unsigned i = 0; i < fmt_ctx_->nb_streams; ++i) 
    {
        if (fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            return i;
    }

    return -1;
}

AVFormatContext* Demux::releaseFormatCtx() 
{
    AVFormatContext* ctx = fmt_ctx_;
    fmt_ctx_ = nullptr;
    return ctx;
}

bool Demux::readPacket(AVPacket *pkt)
{
    // 调用 av_read_frame() 从 AVFormatContext 读取数据
    return av_read_frame(fmt_ctx_, pkt) == 0;
}

void Demux::close() 
{
    if (fmt_ctx_)
    {
        avformat_close_input(&fmt_ctx_);
        fmt_ctx_ = nullptr;
    }
}