#include "opengl_renderer.hpp"
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

OpenGLRenderer::OpenGLRenderer(PlayerState* state) : state_(state) {}

OpenGLRenderer::~OpenGLRenderer() 
{ 
    clear(); 
}

bool OpenGLRenderer::init(int video_width, int video_height, AVPixelFormat pix_fmt) 
{
    if (window_ || gl_context_) 
    {
        std::cerr << "Renderer already initialized" << std::endl;
        return false;
    }
    
    video_width_ = video_width;
    video_height_ = video_height;
    pix_fmt_ = pix_fmt;
    
    // 设置OpenGL属性
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    // 创建窗口
    window_ = SDL_CreateWindow(
        "FFmpeg Player - OpenGL",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        window_width_, window_height_,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    
    if (!window_) 
    {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        state_->set_error(PlayerError::VIDEO_RENDERER_FAILED, 
                         "SDL_CreateWindow failed: " + std::string(SDL_GetError()));
        return false;
    }
    
    // 创建OpenGL上下文
    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) 
    {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        state_->set_error(PlayerError::VIDEO_RENDERER_FAILED,
                         "SDL_GL_CreateContext failed: " + std::string(SDL_GetError()));
        return false;
    }
    
    // 初始化GLAD
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) 
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        state_->set_error(PlayerError::VIDEO_RENDERER_FAILED, "Failed to initialize GLAD");
        return false;
    }
    
    // 设置视口
    glViewport(0, 0, window_width_, window_height_);
    
    // 设置垂直同步
    SDL_GL_SetSwapInterval(1);
    
    // 创建纹理
    createTextures(video_width_, video_height_);
    
    // 设置顶点数据
    setupVertexData();
    
    // 创建着色器
    shader_ = new Shader(
        "shaders/yuv_vertex.glsl",
        "shaders/yuv_fragment.glsl"
    );
    
    if (!shader_ || shader_->ID == 0) 
    {
        std::cerr << "Failed to create shader program" << std::endl;
        return false;
    }
    
    // 如果需要格式转换，创建sws上下文
    if (pix_fmt != AV_PIX_FMT_YUV420P) 
    {
        sws_ctx_ = sws_getContext(
            video_width_, video_height_, pix_fmt,
            video_width_, video_height_, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        
        if (!sws_ctx_) 
        {
            std::cerr << "Failed to create sws context" << std::endl;
        }
    }
    
    return true;
}

void OpenGLRenderer::createTextures(int width, int height) 
{
    // 创建YUV纹理
    glGenTextures(1, &y_texture_);
    glGenTextures(1, &u_texture_);
    glGenTextures(1, &v_texture_);
    
    // 设置Y纹理
    glBindTexture(GL_TEXTURE_2D, y_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    
    // 设置U纹理 (宽度和高度是Y的一半)
    glBindTexture(GL_TEXTURE_2D, u_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width/2, height/2, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    
    // 设置V纹理 (宽度和高度是Y的一半)
    glBindTexture(GL_TEXTURE_2D, v_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width/2, height/2, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLRenderer::setupVertexData()
{
    // 顶点数据 (位置, 纹理坐标)
    float vertices[] = {
        // 位置          // 纹理坐标
        -1.0f,  1.0f,   0.0f, 1.0f,  // 左上
        -1.0f, -1.0f,   0.0f, 0.0f,  // 左下
         1.0f, -1.0f,   1.0f, 0.0f,  // 右下
         1.0f,  1.0f,   1.0f, 1.0f   // 右上
    };
    
    unsigned int indices[] = {
        0, 1, 2,
        0, 2, 3
    };
    
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    
    glBindVertexArray(vao_);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    // 位置属性
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // 纹理坐标属性
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
}

void OpenGLRenderer::calculateDisplayRect(glm::vec4& rect) const
{
    if (video_width_ <= 0 || video_height_ <= 0 || window_width_ <= 0 || window_height_ <= 0) 
    {
        rect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        return;
    }
    
    // 计算视频和窗口的宽高比
    float video_aspect = static_cast<float>(video_width_) / video_height_;
    float window_aspect = static_cast<float>(window_width_) / window_height_;
    
    // 计算显示区域的尺寸（保持宽高比）
    float display_width, display_height;
    
    if (video_aspect > window_aspect) 
    {
        // 视频比窗口宽，以宽度为准
        display_width = window_width_;
        display_height = window_width_ / video_aspect;
    } 
    else 
    {
        // 视频比窗口高，以高度为准
        display_height = window_height_;
        display_width = window_height_ * video_aspect;
    }
    
    // 计算居中位置（归一化坐标）
    rect.x = (window_width_ - display_width) / 2.0f / window_width_;  // 左边距（归一化）
    rect.y = (window_height_ - display_height) / 2.0f / window_height_; // 上边距（归一化）
    rect.z = display_width / window_width_;  // 宽度比例
    rect.w = display_height / window_height_; // 高度比例
}

void OpenGLRenderer::renderFrame(const AVFrame* frame) 
{
    if (!window_ || !frame) return;
    
    // 获取PTS（秒）
    double pts = NAN;
    if (frame->opaque) 
    {
        pts = *((double*)frame->opaque);
    } 
    else if (frame->pts != AV_NOPTS_VALUE) 
    {
        AVStream* stream = state_->fmt_ctx->streams[state_->video_stream];
        pts = frame->pts * av_q2d(stream->time_base);
    }
    
    // 更新视频时钟 - 使用 Clock 类
    if (!std::isnan(pts)) 
    {
        state_->video_clock.set(pts);
    }
    
    // 如果需要格式转换
    AVFrame* converted_frame = const_cast<AVFrame*>(frame);
    if (sws_ctx_) 
    {
        converted_frame = av_frame_alloc();
        converted_frame->format = AV_PIX_FMT_YUV420P;
        converted_frame->width = frame->width;
        converted_frame->height = frame->height;
        converted_frame->pts = frame->pts;
        
        if (av_frame_get_buffer(converted_frame, 0) < 0) 
        {
            av_frame_free(&converted_frame);
            return;
        }
        
        sws_scale(sws_ctx_,
                 frame->data, frame->linesize, 0, frame->height,
                 converted_frame->data, converted_frame->linesize);
    }
    
    // 上传纹理数据
    glBindTexture(GL_TEXTURE_2D, y_texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                   converted_frame->width, converted_frame->height,
                   GL_RED, GL_UNSIGNED_BYTE, converted_frame->data[0]);
    
    glBindTexture(GL_TEXTURE_2D, u_texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                   converted_frame->width/2, converted_frame->height/2,
                   GL_RED, GL_UNSIGNED_BYTE, converted_frame->data[1]);
    
    glBindTexture(GL_TEXTURE_2D, v_texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                   converted_frame->width/2, converted_frame->height/2,
                   GL_RED, GL_UNSIGNED_BYTE, converted_frame->data[2]);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // 清理转换后的帧（如果需要）
    if (sws_ctx_ && converted_frame != frame) 
    {
        av_frame_free(&converted_frame);
    }
    
    // 清除颜色缓冲区
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // 使用着色器程序
    shader_->use();
    
    // 绑定纹理
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, y_texture_);
    shader_->setInt("y_texture", 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, u_texture_);
    shader_->setInt("u_texture", 1);
    
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, v_texture_);
    shader_->setInt("v_texture", 2);
    
    // 计算显示区域
    glm::vec4 display_rect;
    calculateDisplayRect(display_rect);
    shader_->setVec4("display_rect", display_rect);
    
    // 绘制矩形
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // 交换缓冲区
    SDL_GL_SwapWindow(window_);
}

void OpenGLRenderer::clear() 
{
    if (shader_) 
    {
        delete shader_;
        shader_ = nullptr;
    }
    
    if (y_texture_) glDeleteTextures(1, &y_texture_);
    if (u_texture_) glDeleteTextures(1, &u_texture_);
    if (v_texture_) glDeleteTextures(1, &v_texture_);
    
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
    
    if (sws_ctx_) { sws_freeContext(sws_ctx_); sws_ctx_ = nullptr; }
    
    if (gl_context_) { SDL_GL_DeleteContext(gl_context_); gl_context_ = nullptr; }
    if (window_) { SDL_DestroyWindow(window_); window_ = nullptr; }
}

void OpenGLRenderer::handleResize(int width, int height) 
{
    if (window_ && (width != window_width_ || height != window_height_)) 
    {
        window_width_ = width;
        window_height_ = height;
        glViewport(0, 0, width, height);
        
        // 强制重绘一帧来更新显示
        // 这里可以触发一个重绘事件，或者等待下一帧自动更新
        std::cout << "Window resized to: " << width << "x" << height << std::endl;
        
        // 如果可能，触发一个重绘事件
        // SDL_Event event;
        // event.type = SDL_WINDOWEVENT;
        // event.window.event = SDL_WINDOWEVENT_EXPOSED;
        // SDL_PushEvent(&event);
    }
}

void OpenGLRenderer::toggleFullscreen() 
{
    if (!window_) return;
    
    Uint32 flags = SDL_GetWindowFlags(window_);
    if (flags & SDL_WINDOW_FULLSCREEN) 
    {
        SDL_SetWindowFullscreen(window_, 0);
        fullscreen_ = false;
        
        // 恢复窗口大小
        SDL_SetWindowSize(window_, 1280, 720);
        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    } 
    else 
    {
        SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
        fullscreen_ = true;
    }
}

bool OpenGLRenderer::isFullscreen() const 
{
    return fullscreen_;
}