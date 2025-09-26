// opengl_renderer.hpp
#pragma once

#include <SDL2/SDL.h>
#include <glad/glad.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

#include "player_core/player_state.hpp"
#include "shader_utils/shader.hpp"
#include "ui/ui_layer.hpp"

class OpenGLRenderer {
public:
    OpenGLRenderer(PlayerState* state);
    ~OpenGLRenderer();
    
    bool init(int video_width, int video_height, AVPixelFormat pix_fmt);
    void renderFrame(const AVFrame* frame);
    void clear();
    void handleResize(int width, int height);
    void toggleFullscreen();
    
    SDL_Window* getWindow() const { return window_; }
    SDL_GLContext getGLContext() const { return gl_context_; }
    GLuint getRenderTexture() const { return m_renderTexture; }

    // UI 层访问方法
    UiLayer* getUiLayer() { return ui_layer_.get(); }
    
    // 设置视频文件选择回调
    void setOpenVideoCallback(const std::function<void(const std::string&)>& callback);

    bool initForUIOnly(); // 只用于UI显示的初始化
    bool isInitializedForVideo() const  // 检查是否已经为视频播放初始化
    {    
        // 检查是否已经为视频播放初始化
        return (y_texture_ != 0 && u_texture_ != 0 && v_texture_ != 0 && shader_ != nullptr);
    }; 

    // 新增：为新视频更新渲染器（而不是重新创建）
    bool updateForNewVideo(int video_width, int video_height, AVPixelFormat pix_fmt);
    // 检查是否已经初始化了基础OpenGL环境
    bool isOpenGLReady() const { return window_ != nullptr && gl_context_ != nullptr; }
    
    // 事件处理方法
    void handleSDLEvent(const SDL_Event& event);
    void renderUI();
    
private:
    void createTextures(int width, int height);
    void setupVertexData();
    void createFramebuffer(int width, int height);
    void deleteFramebuffer();
    void renderVideoToFBO(const AVFrame* frame);

    // 分离创建窗口和创建视频相关资源
    void clearVideoResources();
    bool createVideoResources(int width, int height, AVPixelFormat pix_fmt);
    
    PlayerState* state_;
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    
    // OpenGL对象
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLuint y_texture_ = 0;
    GLuint u_texture_ = 0;
    GLuint v_texture_ = 0;
    Shader* shader_ = nullptr;
    
    // 帧缓冲对象
    GLuint m_fbo = 0;
    GLuint m_renderTexture = 0;
    
    // 视频参数
    int video_width_ = 0;
    int video_height_ = 0;
    AVPixelFormat pix_fmt_ = AV_PIX_FMT_NONE;
    
    // 窗口参数
    int window_width_ = 1280;
    int window_height_ = 720;
    bool fullscreen_ = false;
    
    // 格式转换
    SwsContext* sws_ctx_ = nullptr;

    // UI 层
    std::unique_ptr<UiLayer> ui_layer_;
    std::function<void(const std::string&)> open_video_callback_;
};