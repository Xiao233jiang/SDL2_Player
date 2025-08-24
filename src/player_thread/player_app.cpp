#include "player_app.hpp"
#include <iostream>
#include <memory>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

#include "../player_core/decode/audio_decode.hpp"
#include "../player_core/decode/video_decode.hpp"

PlayerApp::PlayerApp(const std::string& filename) {
    state_.filename = filename;
}

PlayerApp::~PlayerApp() {
    stop();
}

bool PlayerApp::init() {
    if (initialized_) return true;
    
    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL初始化失败: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // 创建解封装线程
    demux_thread_ = std::make_unique<DemuxThread>(&state_);
    
    // 启动解封装线程
    demux_thread_->start();
    
    // 等待解封装线程准备好
    {
        std::unique_lock<std::mutex> lock(state_.demux_ready_mutex);
        state_.demux_ready_cv.wait(lock, [this]() { return state_.demux_ready.load(); });
    }
    
    // 检查是否有错误
    if (state_.error != PlayerError::NONE) {
        std::cerr << "DemuxThread error: " << state_.error_message << std::endl;
        return false;
    }
    
    // 设置音频和视频解码器
    if (state_.audio_stream >= 0 && !setupAudio()) {
        std::cerr << "Failed to setup audio" << std::endl;
        return false;
    }
    
    if (state_.video_stream >= 0 && !setupVideo()) {
        std::cerr << "Failed to setup video" << std::endl;
        return false;
    }
    
    // 创建解码线程和刷新定时器
    if (!createThreads()) {
        std::cerr << "Failed to create threads" << std::endl;
        return false;
    }
    
    initialized_ = true;
    return true;
}

bool PlayerApp::setupAudio() {
    AVStream* audio_stream = state_.fmt_ctx->streams[state_.audio_stream];
    AVCodecParameters* codecpar = audio_stream->codecpar;
    
    // 查找解码器
    AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        std::cerr << "不支持的音频编解码器" << std::endl;
        return false;
    }
    
    // 分配解码器上下文
    state_.audio_ctx = avcodec_alloc_context3(codec);
    if (!state_.audio_ctx) {
        std::cerr << "无法分配音频解码器上下文" << std::endl;
        return false;
    }
    
    // 复制参数到解码器上下文
    if (avcodec_parameters_to_context(state_.audio_ctx, codecpar) < 0) {
        std::cerr << "无法复制参数到音频解码器上下文" << std::endl;
        return false;
    }
    
    // 打开解码器
    if (avcodec_open2(state_.audio_ctx, codec, nullptr) < 0) {
        std::cerr << "无法打开音频编解码器" << std::endl;
        return false;
    }
    
    // 创建音频播放器 - 适配修改
    audio_player_ = std::make_unique<AudioPlayer>(&state_);
    if (!audio_player_->open()) {  // 移除参数，使用默认配置
        std::cerr << "无法打开音频设备" << std::endl;
        return false;
    }
    
    return true;
}

bool PlayerApp::setupVideo() {
    AVStream* video_stream = state_.fmt_ctx->streams[state_.video_stream];
    AVCodecParameters* codecpar = video_stream->codecpar;
    
    // 查找解码器
    AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        std::cerr << "不支持的视频编解码器" << std::endl;
        return false;
    }
    
    // 分配解码器上下文
    state_.video_ctx = avcodec_alloc_context3(codec);
    if (!state_.video_ctx) {
        std::cerr << "无法分配视频解码器上下文" << std::endl;
        return false;
    }
    
    // 复制参数到解码器上下文
    if (avcodec_parameters_to_context(state_.video_ctx, codecpar) < 0) {
        std::cerr << "无法复制参数到视频解码器上下文" << std::endl;
        return false;
    }
    
    // 打开解码器
    if (avcodec_open2(state_.video_ctx, codec, nullptr) < 0) {
        std::cerr << "无法打开视频编解码器" << std::endl;
        return false;
    }
    
    // 创建渲染器
    renderer_ = std::make_unique<Renderer>(&state_);
    if (!renderer_->init(state_.video_ctx->width, 
                       state_.video_ctx->height, 
                       state_.video_ctx->pix_fmt)) {
        std::cerr << "无法初始化视频渲染器" << std::endl;
        return false;
    }
    
    return true;
}

bool PlayerApp::createThreads() {
    // 创建音频解码线程
    if (state_.audio_stream >= 0) {
        auto audio_decoder = std::make_unique<AudioDecode>(state_.audio_ctx);
        audio_decode_thread_ = std::make_unique<AudioDecodeThread>(
            audio_decoder.release(), 
            &state_.audio_packet_queue, 
            &state_.audio_frame_queue, 
            &state_,
            "AudioDecodeThread"
        );
    }
    
    // 创建视频解码线程
    if (state_.video_stream >= 0) {
        auto video_decoder = std::make_unique<VideoDecode>(state_.video_ctx);
        video_decode_thread_ = std::make_unique<VideoDecodeThread>(
            video_decoder.release(), 
            &state_.video_packet_queue, 
            &state_.video_frame_queue, 
            &state_,
            "VideoDecodeThread"
        );
        
        // 创建视频刷新定时器
        refresh_timer_ = std::make_unique<VideoRefreshTimer>(&state_);
    }
    
    return true;
}

void PlayerApp::run() {
    if (!initialized_) {
        std::cerr << "播放器未初始化" << std::endl;
        return;
    }
    
    // 注意：解封装线程已经在 init() 中启动了，所以这里不再启动
    
    if (audio_decode_thread_) {
        audio_decode_thread_->start();
    }
    
    if (video_decode_thread_) {
        video_decode_thread_->start();
    }
    
    // 启动音频播放
    if (audio_player_) {
        audio_player_->start();
    }
    
    // 启动视频刷新定时器
    if (refresh_timer_) {
        refresh_timer_->start();
    }
    
    // 主事件循环
    handleEvents();
    
    // 停止播放
    stop();
}

void PlayerApp::stop() {
    state_.quit = true;
    
    // 停止所有线程
    if (demux_thread_) {
        demux_thread_->stop();
        demux_thread_->join();
    }
    
    if (audio_decode_thread_) {
        audio_decode_thread_->stop();
        audio_decode_thread_->join();
    }
    
    if (video_decode_thread_) {
        video_decode_thread_->stop();
        video_decode_thread_->join();
    }
    
    if (refresh_timer_) {
        refresh_timer_->stop();
        refresh_timer_->join();
    }
    
    // 停止音频播放
    if (audio_player_) {
        audio_player_->stop();
    }
    
    cleanUp();
    initialized_ = false;
}

void PlayerApp::handleEvents() {
    SDL_Event event;
    
    while (!state_.quit && SDL_WaitEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                state_.quit = true;
                break;
                
            case FF_REFRESH_EVENT:
                videoRefresh();
                break;
                
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    if (renderer_) {
                        renderer_->handleResize(event.window.data1, event.window.data2);
                    }
                }
                break;
                
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        state_.quit = true;
                        break;
                    case SDLK_f:
                        if (renderer_) {
                            renderer_->toggleFullscreen();
                        }
                        break;
                    default:
                        break;
                }
                break;
                
            default:
                break;
        }
    }
}

void PlayerApp::videoRefresh() {
    if (!renderer_) return;
    
    AVFrame* frame = nullptr;
    if (state_.video_frame_queue.pop(frame, state_.quit, 10)) {
        renderer_->renderFrame(frame);
        av_frame_free(&frame);
    }
}

void PlayerApp::cleanUp() {
    // 清理渲染器
    if (renderer_) {
        renderer_->clear();
        renderer_.reset();
    }
    
    // 清理音频播放器
    if (audio_player_) {
        audio_player_.reset();
    }
    
    // 清理线程对象
    demux_thread_.reset();
    audio_decode_thread_.reset();
    video_decode_thread_.reset();
    refresh_timer_.reset();
    
    // 清理状态
    state_.clear();
    
    SDL_Quit();
}