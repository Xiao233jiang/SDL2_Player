// player_app.cpp
#include "player_app.hpp"
#include "play/opengl_renderer.hpp"
#include <iostream>
#include <memory>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

#include "player_core/decode/audio_decode.hpp"
#include "player_core/decode/video_decode.hpp"

PlayerApp::PlayerApp(const std::string& filename) 
{
    state_.filename = filename;
    // 即使没有文件名也可以启动
}

PlayerApp::~PlayerApp() 
{
    stop();
    
    // 完全清理，包括SDL
    if (renderer_) {
        renderer_->clear();
        renderer_.reset();
    }
    
    state_.clear();
    SDL_Quit(); // 只在这里调用SDL_Quit()
}

bool PlayerApp::init() 
{
    if (initialized_) return true;
    
    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) 
    {
        std::cerr << "SDL初始化失败: " << SDL_GetError() << std::endl;
        return false;
    }

    // 初始化时钟
    state_.audio_clock.set(0);
    state_.video_clock.set(0);

    // 如果没有文件名，直接创建渲染器用于显示UI
    if (state_.filename.empty()) {
        // 创建一个基础的OpenGL窗口用于显示UI
        renderer_ = std::make_unique<OpenGLRenderer>(&state_);
        if (!renderer_->initForUIOnly()) { // 需要添加这个方法
            std::cerr << "Failed to initialize renderer for UI" << std::endl;
            return false;
        }
        
        // 设置文件选择回调
        renderer_->setOpenVideoCallback([this](const std::string& path) {
            this->openVideo(path);
        });
        
        initialized_ = true;
        return true;
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
    if (state_.error != PlayerError::NONE) 
    {
        std::cerr << "DemuxThread error: " << state_.error_message << std::endl;
        return false;
    }
    
    // 设置音频和视频解码器
    if (state_.audio_stream >= 0 && !setupAudio()) 
    {
        std::cerr << "Failed to setup audio" << std::endl;
        return false;
    }
    
    if (state_.video_stream >= 0 && !setupVideo()) 
    {
        std::cerr << "Failed to setup video" << std::endl;
        return false;
    }
    
    // 创建解码线程和刷新定时器
    if (!createThreads()) 
    {
        std::cerr << "Failed to create threads" << std::endl;
        return false;
    }

    // 设置视频文件选择回调 - 修改为直接传递给渲染器
    if (renderer_) {
        renderer_->setOpenVideoCallback([this](const std::string& path) {
            this->openVideo(path);
        });
    }
    
    initialized_ = true;
    return true;
}

bool PlayerApp::setupAudio() 
{
    AVStream* audio_stream = state_.fmt_ctx->streams[state_.audio_stream];
    AVCodecParameters* codecpar = audio_stream->codecpar;
    
    // 查找解码器
    AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) 
    {
        std::cerr << "不支持的音频编解码器" << std::endl;
        return false;
    }
    
    // 分配解码器上下文
    state_.audio_ctx = avcodec_alloc_context3(codec);
    if (!state_.audio_ctx) 
    {
        std::cerr << "无法分配音频解码器上下文" << std::endl;
        return false;
    }
    
    // 复制参数到解码器上下文
    if (avcodec_parameters_to_context(state_.audio_ctx, codecpar) < 0) 
    {
        std::cerr << "无法复制参数到音频解码器上下文" << std::endl;
        return false;
    }
    
    // 打开解码器
    if (avcodec_open2(state_.audio_ctx, codec, nullptr) < 0) 
    {
        std::cerr << "无法打开音频编解码器" << std::endl;
        return false;
    }

    // 在创建音频播放器之前，确保音频上下文有效
    if (!state_.audio_ctx || state_.audio_ctx->sample_rate <= 0) 
    {
        std::cerr << "无效的音频编解码器上下文" << std::endl;
        return false;
    }
    
    // 创建音频播放器
    audio_player_ = std::make_unique<AudioPlayer>(&state_);
    if (!audio_player_->open()) 
    {
        std::cerr << "无法打开音频设备" << std::endl;
        return false;
    }
    
    return true;
}

bool PlayerApp::setupVideo() 
{
    AVStream* video_stream = state_.fmt_ctx->streams[state_.video_stream];
    AVCodecParameters* codecpar = video_stream->codecpar;
    
    // 查找解码器
    AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) 
    {
        std::cerr << "不支持的视频编解码器" << std::endl;
        return false;
    }
    
    // 分配解码器上下文
    state_.video_ctx = avcodec_alloc_context3(codec);
    if (!state_.video_ctx) 
    {
        std::cerr << "无法分配视频解码器上下文" << std::endl;
        return false;
    }
    
    // 复制参数到解码器上下文
    if (avcodec_parameters_to_context(state_.video_ctx, codecpar) < 0) 
    {
        std::cerr << "无法复制参数到视频解码器上下文" << std::endl;
        return false;
    }
    
    // 打开解码器
    if (avcodec_open2(state_.video_ctx, codec, nullptr) < 0) 
    {
        std::cerr << "无法打开视频编解码器" << std::endl;
        return false;
    }
    
    // 修改：不重新创建渲染器，而是更新现有渲染器
    if (!renderer_) {
        // 只有在没有渲染器时才创建新的
        renderer_ = std::make_unique<OpenGLRenderer>(&state_);
        if (!renderer_->init(state_.video_ctx->width, 
                           state_.video_ctx->height, 
                           state_.video_ctx->pix_fmt)) 
        {
            std::cerr << "无法初始化视频渲染器" << std::endl;
            return false;
        }
    } else if (renderer_->isOpenGLReady()) {
        // 渲染器已存在且OpenGL环境就绪，只需要更新视频资源
        if (!renderer_->updateForNewVideo(state_.video_ctx->width, 
                                        state_.video_ctx->height, 
                                        state_.video_ctx->pix_fmt)) {
            std::cerr << "无法更新视频渲染器" << std::endl;
            return false;
        }
        
        // 重要：更新UI层的视频尺寸信息
        if (renderer_->getUiLayer()) {
            renderer_->getUiLayer()->SetVideoSize(state_.video_ctx->width, state_.video_ctx->height);
            std::cout << "Updated UI with video size: " << state_.video_ctx->width << "x" << state_.video_ctx->height << std::endl;
        }
    } else {
        std::cerr << "渲染器状态异常" << std::endl;
        return false;
    }
    
    return true;
}

bool PlayerApp::createThreads() 
{
    // 创建音频解码线程
    if (state_.audio_stream >= 0) 
    {
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
    if (state_.video_stream >= 0) 
    {
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

void PlayerApp::run() 
{
    if (!initialized_) 
    {
        std::cerr << "播放器未初始化" << std::endl;
        return;
    }
    
    // 如果没有文件，只运行UI
    if (state_.filename.empty()) {
        std::cout << "No file loaded, running UI only" << std::endl;
        handleEvents();
        return;
    }
    
    // 有文件时启动播放
    std::cout << "Starting playback for: " << state_.filename << std::endl;
    startPlayback();
    
    handleEvents();
    stop();
}

void PlayerApp::stop() 
{
    state_.quit = true;
    
    // 停止所有线程
    if (demux_thread_) 
    {
        demux_thread_->stop();
        demux_thread_->join();
    }
    
    if (audio_decode_thread_) 
    {
        audio_decode_thread_->stop();
        audio_decode_thread_->join();
    }
    
    if (video_decode_thread_) 
    {
        video_decode_thread_->stop();
        video_decode_thread_->join();
    }
    
    if (refresh_timer_) 
    {
        refresh_timer_->stop();
        refresh_timer_->join();
    }
    
    // 停止音频播放
    if (audio_player_) 
    {
        audio_player_->stop();
    }
    
    // 等待所有线程真正结束
    state_.wait_for_threads(5000);  // 等待5秒
    
    cleanUp();
    initialized_ = false;
}

void PlayerApp::handleEvents() {
    SDL_Event event;
    
    while (!state_.quit) {
        while (SDL_PollEvent(&event)) {
            // 传递事件给渲染器（用于 UI 处理）
            if (renderer_) {
                renderer_->handleSDLEvent(event);
            }
            
            switch (event.type) {
                case FF_REFRESH_EVENT:
                    videoRefresh();
                    break;
                    
                case SDL_QUIT:
                    state_.quit = true;
                    break;
                    
                case SDL_KEYDOWN:
                    handleKeyPress(event.key.keysym.sym);
                    break;
                    
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED && renderer_) {
                        renderer_->handleResize(event.window.data1, event.window.data2);
                    }
                    break;
                    
                default:
                    break;
            }
        }
        
        // 渲染
        if (renderer_) {
            renderer_->renderUI();
        }
        
        SDL_Delay(1);
    }
}

void PlayerApp::handleKeyPress(SDL_Keycode key) {
    switch (key) {
        case SDLK_LEFT:
            state_.doSeekRelative(-5.0); // 左箭头：后退 5 秒
            break;
        case SDLK_RIGHT:
            state_.doSeekRelative(5.0);  // 右箭头：前进 5 秒
            break;
        case SDLK_DOWN:
            state_.doSeekRelative(-60.0); // 下箭头：后退 1 分钟
            break;
        case SDLK_UP:
            state_.doSeekRelative(60.0);  // 上箭头：前进 1 分钟
            break;
        case SDLK_SPACE:
            // 播放/暂停（需要实现正确的播放状态控制）
            printf("Space key pressed - implement play/pause\n");
            break;
        case SDLK_f:
            if (renderer_) {
                renderer_->toggleFullscreen();
            }
            break;
        default:
            break;
    }
}

void PlayerApp::videoRefresh()
{
    if (!renderer_) return;
    
    AVFrame* frame = nullptr;
    if (!state_.video_frame_queue.try_pop(frame) || !frame) {
        return;
    }
    
    // 如果正在 seeking，直接渲染不进行时间同步
    if (state_.seeking.load()) {
        std::cout << "Seeking in progress, rendering frame without sync" << std::endl;
        renderer_->renderFrame(frame);
        av_frame_free(&frame);
        renderer_->renderUI();
        return;
    }
    
    // 计算视频PTS
    double video_pts = 0.0;
    bool has_pts = false;
    
    if (frame->opaque) {
        video_pts = *((double*)frame->opaque);
        has_pts = true;
    }
    
    if (has_pts) {
        // 获取音频时钟作为主时钟
        double master_clock = state_.get_master_clock();
        double diff = video_pts - master_clock;
        
        // 同步阈值
        const double sync_threshold = 0.04; // 40ms
        
        if (diff < -sync_threshold) {
            // 视频落后太多，跳过这一帧
            // std::cout << "Video lagging, skipping frame. Diff: " << diff << "s" << std::endl;
            av_frame_free(&frame);
            return;
        } else if (diff > sync_threshold) {
            // 视频超前，暂时不处理这一帧，放回队列
            // std::cout << "Video ahead, delaying frame. Diff: " << diff << "s" << std::endl;
            av_frame_free(&frame);
            return;
        }
        
        // 更新视频时钟
        state_.video_clock.set(video_pts);
        
        // std::cout << "Rendering frame - Video PTS: " << video_pts 
        //           << "s, Audio Clock: " << master_clock 
        //           << "s, Diff: " << diff << "s" << std::endl;
    }
    
    // 渲染视频帧
    renderer_->renderFrame(frame);
    av_frame_free(&frame);
    
    // 强制渲染UI以更新ImGui
    renderer_->renderUI();
}

void PlayerApp::openVideo(const std::string& filename)
{
    std::cout << "Opening video file: " << filename << std::endl;
    
    // 设置加载状态
    state_.loading.store(true);
    
    // 立即清理UI中的视频信息
    if (renderer_ && renderer_->getUiLayer()) {
        renderer_->getUiLayer()->ClearVideoInfo();
    }
    
    // 手动清理但不设置quit标志，也不重置渲染器
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
    
    if (audio_player_) {
        audio_player_->stop();
    }
    
    // 重置状态但不退出程序
    state_.resetForNewFile();
    state_.filename = filename;
    
    // 清理之前的线程对象，但保留渲染器
    demux_thread_.reset();
    audio_decode_thread_.reset();
    video_decode_thread_.reset();
    audio_player_.reset();
    refresh_timer_.reset();
    
    // 重新初始化
    bool old_initialized = initialized_;
    initialized_ = false;
    if (initWithFile()) {
        std::cout << "Video file loaded successfully, starting playback..." << std::endl;
        startPlayback();
        initialized_ = true;
        state_.loading.store(false); // 加载完成
    } else {
        std::cerr << "Failed to load video file: " << filename << std::endl;
        state_.filename.clear();
        state_.loading.store(false); // 加载失败
        
        // 失败时清理UI
        if (renderer_ && renderer_->getUiLayer()) {
            renderer_->getUiLayer()->ClearVideoInfo();
        }
        
        initialized_ = old_initialized;
    }
}

// 专门的播放启动方法
void PlayerApp::startPlayback() {
    if (audio_decode_thread_) {
        audio_decode_thread_->start();
    }
    
    if (video_decode_thread_) {
        video_decode_thread_->start();
    }
    
    if (audio_player_) {
        audio_player_->start();
    }
    
    if (refresh_timer_) {
        refresh_timer_->start();
    }
}

bool PlayerApp::initWithFile() {
    // 创建解封装线程但不启动
    demux_thread_ = std::make_unique<DemuxThread>(&state_);
    
    // 启动解封装线程
    demux_thread_->start();
    
    // 等待解封装准备就绪
    {
        std::unique_lock<std::mutex> lock(state_.demux_ready_mutex);
        state_.demux_ready_cv.wait_for(lock, std::chrono::seconds(10), 
            [this]() { return state_.demux_ready.load() || state_.quit.load(); });
    }
    
    if (state_.quit.load()) {
        std::cerr << "解封装失败或被取消" << std::endl;
        return false;
    }
    
    // 检查解封装是否成功
    if (state_.error.load() != PlayerError::NONE) {
        std::cerr << "DemuxThread error: " << state_.error_message << std::endl;
        return false;
    }
    
    // 设置音频和视频解码器
    if (state_.audio_stream >= 0 && !setupAudio()) 
    {
        std::cerr << "Failed to setup audio" << std::endl;
        return false;
    }
    
    if (state_.video_stream >= 0 && !setupVideo()) 
    {
        std::cerr << "Failed to setup video" << std::endl;
        return false;
    }
    
    // 创建解码线程和刷新定时器（但不启动）
    if (!createThreads()) 
    {
        std::cerr << "Failed to create threads" << std::endl;
        return false;
    }
    
    return true;
}

void PlayerApp::cleanUp() {
    // 只清理资源，不要调用 SDL_Quit()，因为我们还要继续使用SDL
    
    // 清理其他资源
    if (audio_player_) {
        audio_player_.reset();
    }
    
    // 清理线程对象
    demux_thread_.reset();
    audio_decode_thread_.reset();
    video_decode_thread_.reset();
    refresh_timer_.reset();
    
    // 不要重置渲染器，它还要继续使用
    // 不要清理状态，因为UI还需要它
    
    // 不要调用 SDL_Quit()!
}