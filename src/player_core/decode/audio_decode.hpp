#pragma once
#include "../decode.hpp"

class AudioDecode : public Decode 
{
public:
    AudioDecode() = default;
    explicit AudioDecode(AVCodecContext* codec_ctx);
    ~AudioDecode() override = default;
};