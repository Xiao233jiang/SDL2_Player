#include "renderer.hpp"
#include <iostream>

Renderer::Renderer(PlayerState* state) : state_(state) {}

Renderer::~Renderer() 
{ 
    clear(); 
}

bool Renderer::init(int video_width, int video_height, AVPixelFormat pix_fmt) 
{
    if (window_ || renderer_) {
        std::cerr << "Renderer already initialized" << std::endl;
        return false;
    }
    
    video_width_ = video_width;
    video_height_ = video_height;
    pix_fmt_ = pix_fmt;
    
    // 创建固定大小的窗口
    window_ = SDL_CreateWindow(
        "FFmpeg Player",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        window_width_, window_height_,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    
    if (!window_) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        state_->set_error(PlayerError::VIDEO_RENDERER_FAILED, 
                         "SDL_CreateWindow failed: " + std::string(SDL_GetError()));
        return false;
    }
    
    // 创建渲染器
    renderer_ = SDL_CreateRenderer(
        window_, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    
    if (!renderer_) {
        std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
        state_->set_error(PlayerError::VIDEO_RENDERER_FAILED,
                         "SDL_CreateRenderer failed: " + std::string(SDL_GetError()));
        return false;
    }
    
    // 创建纹理（使用视频原始尺寸）
    createTexture(video_width_, video_height_, pix_fmt_);
    
    if (!texture_) {
        std::cerr << "Failed to create texture" << std::endl;
        return false;
    }
    
    return true;
}

void Renderer::createTexture(int width, int height, AVPixelFormat pix_fmt) 
{
    Uint32 sdl_format;
    switch (pix_fmt) 
    {
        case AV_PIX_FMT_YUV420P:
            sdl_format = SDL_PIXELFORMAT_IYUV;
            break;
        case AV_PIX_FMT_NV12:
            sdl_format = SDL_PIXELFORMAT_NV12;
            break;
        case AV_PIX_FMT_YUYV422:
            sdl_format = SDL_PIXELFORMAT_YUY2;
            break;
        default:
            sdl_format = SDL_PIXELFORMAT_YV12;
            break;
    }
    
    texture_ = SDL_CreateTexture(
        renderer_, sdl_format,
        SDL_TEXTUREACCESS_STREAMING, width, height
    );
    
    // 需要格式转换时才创建 sws_ctx
    if (sdl_format == SDL_PIXELFORMAT_YV12 && pix_fmt != AV_PIX_FMT_YUV420P) 
    {
        sws_ctx_ = sws_getContext(
            width, height, pix_fmt,
            width, height, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        
        if (!sws_ctx_) {
            std::cerr << "Failed to create sws context" << std::endl;
        }
    }
}

void Renderer::calculateDisplayRect(SDL_Rect* rect) const
{
    if (!rect) return;
    
    // 计算宽高比
    float aspect_ratio = static_cast<float>(video_width_) / video_height_;
    
    // 计算在窗口中的显示区域（保持宽高比）
    int display_width = window_width_;
    int display_height = static_cast<int>(display_width / aspect_ratio);
    
    if (display_height > window_height_) {
        display_height = window_height_;
        display_width = static_cast<int>(display_height * aspect_ratio);
    }
    
    // 居中显示
    rect->x = (window_width_ - display_width) / 2;
    rect->y = (window_height_ - display_height) / 2;
    rect->w = display_width;
    rect->h = display_height;
}

void Renderer::renderFrame(const AVFrame* frame) 
{
    if (!renderer_ || !texture_ || !frame) return;
    
    if (sws_ctx_) 
    {
        // 需要格式转换
        AVFrame* converted_frame = av_frame_alloc();
        converted_frame->format = AV_PIX_FMT_YUV420P;
        converted_frame->width = frame->width;
        converted_frame->height = frame->height;
        converted_frame->pts = frame->pts; // 保持时间戳
        
        if (av_frame_get_buffer(converted_frame, 0) < 0) 
        {
            av_frame_free(&converted_frame);
            return;
        }
        
        sws_scale(sws_ctx_,
                 frame->data, frame->linesize, 0, frame->height,
                 converted_frame->data, converted_frame->linesize);
        
        SDL_UpdateYUVTexture(texture_, nullptr,
                            converted_frame->data[0], converted_frame->linesize[0],
                            converted_frame->data[1], converted_frame->linesize[1],
                            converted_frame->data[2], converted_frame->linesize[2]);
        
        av_frame_free(&converted_frame);
    } 
    else 
    {
        // 直接更新纹理
        SDL_UpdateYUVTexture(texture_, nullptr,
                            frame->data[0], frame->linesize[0],
                            frame->data[1], frame->linesize[1],
                            frame->data[2], frame->linesize[2]);
    }
    
    SDL_RenderClear(renderer_);
    
    // 计算显示区域（保持宽高比）
    SDL_Rect dst_rect;
    calculateDisplayRect(&dst_rect);
    
    // 渲染到计算好的区域
    SDL_RenderCopy(renderer_, texture_, nullptr, &dst_rect);
    SDL_RenderPresent(renderer_);
    
    // 更新视频时钟 - 修复时间戳计算
    double pts;
    if (frame->pts != AV_NOPTS_VALUE) 
    {
        // 使用视频流的时间基准
        AVStream* stream = state_->fmt_ctx->streams[state_->video_stream];
        pts = frame->pts * av_q2d(stream->time_base);
    } 
    else 
    {
        // 如果没有PTS，基于帧率估算
        static double video_clock = 0;
        AVStream* stream = state_->fmt_ctx->streams[state_->video_stream];
        double frame_duration = 1.0 / av_q2d(av_guess_frame_rate(state_->fmt_ctx, stream, nullptr));
        pts = video_clock;
        video_clock += frame_duration;
    }
    state_->update_video_clock(pts);
}

void Renderer::clear() 
{
    if (texture_) { SDL_DestroyTexture(texture_); texture_ = nullptr; }
    if (renderer_) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (window_) { SDL_DestroyWindow(window_); window_ = nullptr; }
    if (sws_ctx_) { sws_freeContext(sws_ctx_); sws_ctx_ = nullptr; }
}

void Renderer::handleResize(int width, int height) 
{
    if (window_ && (width != window_width_ || height != window_height_)) {
        window_width_ = width;
        window_height_ = height;
        
        // 不需要重新创建纹理，只需要重新计算显示区域
        // 纹理保持视频原始尺寸，渲染时会自动缩放
    }
}

void Renderer::toggleFullscreen() 
{
    if (!window_) return;
    
    Uint32 flags = SDL_GetWindowFlags(window_);
    if (flags & SDL_WINDOW_FULLSCREEN) {
        SDL_SetWindowFullscreen(window_, 0);
        fullscreen_ = false;
        
        // 恢复窗口大小
        SDL_SetWindowSize(window_, 1280, 720);
        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    } else {
        SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
        fullscreen_ = true;
    }
}

bool Renderer::isFullscreen() const 
{
    return fullscreen_;
}