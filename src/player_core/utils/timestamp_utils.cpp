#include "timestamp_utils.hpp"

int64_t TimestampUtils::rescaleTimestamp(int64_t timestamp, 
                                        const AVRational& from_time_base,
                                        const AVRational& to_time_base) 
{
    if (timestamp == AV_NOPTS_VALUE) return AV_NOPTS_VALUE;
    return av_rescale_q(timestamp, from_time_base, to_time_base);
}

bool TimestampUtils::isValidTimestamp(int64_t timestamp) 
{
    return timestamp != AV_NOPTS_VALUE;
}

int64_t TimestampUtils::generateTimestamp(int64_t frame_number, 
                                         const AVRational& time_base,
                                         double frame_rate) 
{
    if (frame_rate <= 0) frame_rate = 30.0; // 默认30fps
    return av_rescale_q(frame_number, 
                       av_inv_q(av_d2q(frame_rate, 1000000)), 
                       time_base);
}

void TimestampUtils::copyTimestamps(const AVPacket* pkt, AVFrame* frame,
                                   const AVRational& stream_time_base,
                                   const AVRational& codec_time_base) 
{
    if (pkt->pts != AV_NOPTS_VALUE) 
    {
        frame->pts = rescaleTimestamp(pkt->pts, stream_time_base, codec_time_base);
    } 
    else if (pkt->dts != AV_NOPTS_VALUE) 
    {
        frame->pts = rescaleTimestamp(pkt->dts, stream_time_base, codec_time_base);
    }
    // 如果都没有，保持frame->pts为AV_NOPTS_VALUE
}