#pragma once
#include <SDL2/SDL.h>
#include "../ffmpeg_utils/ffmpeg_headers.hpp"
#include "../player_core/player_state.hpp"

/**
 * Renderer 封装 SDL 渲染器、纹理和像素格式转换。
 */
class Renderer 
{
public:
    Renderer(PlayerState* state);
    ~Renderer();
    
    bool init(int video_width, int video_height, AVPixelFormat pix_fmt);
    void renderFrame(const AVFrame* frame);
    void clear();
    
    // 窗口管理
    void handleResize(int width, int height);
    void toggleFullscreen();
    bool isFullscreen() const;
    
    // 获取视频原始尺寸
    int getVideoWidth() const { return video_width_; }
    int getVideoHeight() const { return video_height_; }
    
    SDL_Window* getWindow() const { return window_; }
    
private:
    PlayerState* state_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    SwsContext* sws_ctx_ = nullptr;
    int window_width_ = 1280;  // 固定窗口宽度
    int window_height_ = 720;  // 固定窗口高度
    int video_width_ = 0;      // 视频原始宽度
    int video_height_ = 0;     // 视频原始高度
    AVPixelFormat pix_fmt_ = AV_PIX_FMT_NONE;
    bool fullscreen_ = false;
    
    void createTexture(int width, int height, AVPixelFormat pix_fmt);
    void calculateDisplayRect(SDL_Rect* rect) const;
};