#pragma once

#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../ffmpeg_utils/ffmpeg_headers.hpp"
#include "../player_core/player_state.hpp"
#include "../shader_utils/shader.hpp"

/**
 * OpenGLRenderer 使用OpenGL 3.3进行视频渲染
 */
class OpenGLRenderer 
{
public:
    OpenGLRenderer(PlayerState* state);
    ~OpenGLRenderer();
    
    bool init(int video_width, int video_height, AVPixelFormat pix_fmt);
    void renderFrame(const AVFrame* frame);
    void clear();
    
    // 窗口管理
    void handleResize(int width, int height);       // 窗口大小改变时调用
    void toggleFullscreen();                        // 切换全屏模式
    bool isFullscreen() const;                      // 是否处于全屏模式
    
    // 获取视频原始尺寸
    int getVideoWidth() const { return video_width_; }
    int getVideoHeight() const { return video_height_; }
    
    SDL_Window* getWindow() const { return window_; }
    
private:
    PlayerState* state_;
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    
    // OpenGL资源
    GLuint vao_, vbo_, ebo_;
    GLuint y_texture_, u_texture_, v_texture_;
    Shader* shader_ = nullptr;
    
    SwsContext* sws_ctx_ = nullptr;
    int window_width_ = 1280;
    int window_height_ = 720;
    int video_width_ = 0;
    int video_height_ = 0;
    AVPixelFormat pix_fmt_ = AV_PIX_FMT_NONE;
    bool fullscreen_ = false;
    
    void createTextures(int width, int height);
    void calculateDisplayRect(glm::vec4& rect) const;
    void setupVertexData();
};