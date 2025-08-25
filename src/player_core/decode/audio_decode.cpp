#include "audio_decode.hpp"

AudioDecode::AudioDecode(AVCodecContext* codec_ctx) 
{
    codec_ctx_ = codec_ctx;
    is_external_ctx_ = true; // 标记为外部传入的上下文
}