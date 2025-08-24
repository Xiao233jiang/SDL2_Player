#pragma once

#include "../../ffmpeg_utils/ffmpeg_headers.hpp"

/**
 * 时间戳处理工具函数
 */
class TimestampUtils 
{
public:
    // 将时间戳从一个时间基准转换到另一个时间基准
    static int64_t rescaleTimestamp(int64_t timestamp, 
                                   const AVRational& from_time_base,
                                   const AVRational& to_time_base);
    
    // 检查时间戳是否有效
    static bool isValidTimestamp(int64_t timestamp);
    
    // 生成有效的时间戳（基于帧数和帧率）
    static int64_t generateTimestamp(int64_t frame_number, 
                                    const AVRational& time_base,
                                    double frame_rate);
    
    // 从数据包复制时间戳到帧
    static void copyTimestamps(const AVPacket* pkt, AVFrame* frame,
                              const AVRational& stream_time_base,
                              const AVRational& codec_time_base);
};