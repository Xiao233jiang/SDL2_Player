#pragma once
#include "../decode.hpp"

class VideoDecode : public Decode 
{
public:
    VideoDecode() = default;
    explicit VideoDecode(AVCodecContext* codec_ctx);
    ~VideoDecode() override = default;
};