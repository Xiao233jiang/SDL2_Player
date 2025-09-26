#pragma once

#include <SDL2/SDL.h>
#include <mutex>

extern "C" {
#include <libavformat/avformat.h>
}

/**
 * 防止 SDL 更改 main() 函数的执行。
 */
#ifdef __MINGW32__
#undef main
#endif

// 队列容量限制
constexpr int MAX_AUDIO_PACKETS = 500;    // 增加容量
constexpr int MAX_VIDEO_PACKETS = 500;    // 增加容量
constexpr int MAX_AUDIO_FRAMES = 100;     // 增加容量
constexpr int MAX_VIDEO_FRAMES = 50;      // 增加容量

// SDL 音频设置
constexpr int SDL_AUDIO_BUFFER_SIZE = 1024;
constexpr int MAX_AUDIO_FRAME_SIZE = 192000;

// 自定义事件
constexpr int FF_REFRESH_EVENT = SDL_USEREVENT;
constexpr int FF_QUIT_EVENT = SDL_USEREVENT + 1;
constexpr int FF_ERROR_EVENT = SDL_USEREVENT + 2;

// Seek 相关常量 - 新增
#ifndef AV_ERROR_MAX_STRING_SIZE
#define AV_ERROR_MAX_STRING_SIZE 64
#endif

// ✅ 修复：统一定义 flush 包标识
constexpr int FF_FLUSH_PACKET_STREAM_INDEX = -999;

// 错误代码
enum class PlayerError 
{
    NONE,
    DEMUX_FAILED,
    AUDIO_CODEC_NOT_FOUND,
    VIDEO_CODEC_NOT_FOUND,
    SDL_INIT_FAILED,
    OUT_OF_MEMORY,
    FILE_OPEN_FAILED,
    STREAM_INFO_FAILED,
    AUDIO_DEVICE_FAILED,
    VIDEO_RENDERER_FAILED,
    RESAMPLER_FAILED,
    SEEK_FAILED  // 新增
};

// 时间常量
constexpr int DEFAULT_VIDEO_INTERVAL_MS = 16; // ~60fps，更频繁的检查
constexpr int DEFAULT_AUDIO_BUFFER_MS = 100;
constexpr double AV_NOSYNC_THRESHOLD = 10.0;

// 性能监控
constexpr int STATS_UPDATE_INTERVAL_MS = 1000;