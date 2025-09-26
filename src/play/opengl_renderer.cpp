#include "opengl_renderer.hpp"
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

OpenGLRenderer::OpenGLRenderer(PlayerState* state) : state_(state) {
    // 创建UI层实例
    ui_layer_ = std::make_unique<UiLayer>(this);
}

OpenGLRenderer::~OpenGLRenderer() { 
    clear(); 
}

bool OpenGLRenderer::init(int video_width, int video_height, AVPixelFormat pix_fmt) {
    if (window_ || gl_context_) {
        std::cerr << "Renderer already initialized" << std::endl;
        return false;
    }
    
    video_width_ = video_width;
    video_height_ = video_height;
    pix_fmt_ = pix_fmt;
    
    // 设置OpenGL属性
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
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
    
    if (!window_) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // 创建OpenGL上下文
    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // 初始化GLAD
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
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
    
    // 创建FBO
    createFramebuffer(video_width_, video_height_);
    
    if (!shader_ || shader_->ID == 0) {
        std::cerr << "Failed to create shader program" << std::endl;
        return false;
    }
    
    // 如果需要格式转换，创建sws上下文
    if (pix_fmt != AV_PIX_FMT_YUV420P) {
        sws_ctx_ = sws_getContext(
            video_width_, video_height_, pix_fmt,
            video_width_, video_height_, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        
        if (!sws_ctx_) {
            std::cerr << "Failed to create sws context" << std::endl;
        }
    }

    // 初始化 UI 层
    if (!ui_layer_->Init(window_, gl_context_)) {
        std::cerr << "Failed to initialize UI layer" << std::endl;
        return false;
    }
    
    // 设置视频尺寸
    ui_layer_->SetVideoSize(video_width_, video_height_);
    
    // 设置播放器状态
    ui_layer_->SetPlayerState(state_);
    
    return true;
}

void OpenGLRenderer::createTextures(int width, int height) 
{
    std::cout << "Creating textures: " << width << "x" << height << std::endl;
    
    // 创建YUV纹理
    glGenTextures(1, &y_texture_);
    glGenTextures(1, &u_texture_);
    glGenTextures(1, &v_texture_);
    
    // 设置Y纹理（全分辨率）
    glBindTexture(GL_TEXTURE_2D, y_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    
    // 设置U纹理（1/4分辨率）
    glBindTexture(GL_TEXTURE_2D, u_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width/2, height/2, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    
    // 设置V纹理（1/4分辨率）
    glBindTexture(GL_TEXTURE_2D, v_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width/2, height/2, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLRenderer::setupVertexData()
{
    // 全屏四边形顶点数据 (位置, 纹理坐标)
    float vertices[] = {
        // 位置          // 纹理坐标
        -1.0f,  1.0f,   0.0f, 1.0f,  // 左上
        -1.0f, -1.0f,   0.0f, 0.0f,  // 左下
         1.0f, -1.0f,   1.0f, 0.0f,  // 右下
         1.0f,  1.0f,   1.0f, 1.0f   // 右上
    };
    
    unsigned int indices[] = {
        0, 1, 2,  // 第一个三角形
        0, 2, 3   // 第二个三角形
    };
    
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    
    glBindVertexArray(vao_);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    // 位置属性 (location = 0)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // 纹理坐标属性 (location = 1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
}

void OpenGLRenderer::renderFrame(const AVFrame* frame) 
{
    if (!window_ || !frame || !shader_) {
        std::cerr << "Missing components - window: " << (window_ != nullptr) 
                  << " frame: " << (frame != nullptr) 
                  << " shader: " << (shader_ != nullptr) << std::endl;
        return;
    }
    
    // 验证数据
    uint8_t* y_data = frame->data[0];
    uint8_t* u_data = frame->data[1];
    uint8_t* v_data = frame->data[2];
    
    if (!y_data || !u_data || !v_data) {
        std::cerr << "Invalid frame data pointers" << std::endl;
        return;
    }
    
    // std::cout << "Y data samples: " << (int)y_data[0] << " " << (int)y_data[100] << " " << (int)y_data[1000] << std::endl;
    
    // 提前声明所有变量，避免goto跳过初始化
    GLint program_id = 0;
    GLint vao_binding = 0;
    GLenum error = GL_NO_ERROR;
    
    // 1. 绑定FBO并设置视口
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, video_width_, video_height_);
    
    // 清除为蓝色背景
    glClearColor(0.2f, 0.3f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // 2. 上传纹理数据（简化版）
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, y_texture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int y = 0; y < frame->height; y++) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, frame->width, 1,
                       GL_RED, GL_UNSIGNED_BYTE, 
                       frame->data[0] + y * frame->linesize[0]);
    }
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, u_texture_);
    for (int y = 0; y < frame->height/2; y++) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, frame->width/2, 1,
                       GL_RED, GL_UNSIGNED_BYTE, 
                       frame->data[1] + y * frame->linesize[1]);
    }
    
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, v_texture_);
    for (int y = 0; y < frame->height/2; y++) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, frame->width/2, 1,
                       GL_RED, GL_UNSIGNED_BYTE, 
                       frame->data[2] + y * frame->linesize[2]);
    }
    
    // 3. 使用着色器程序
    if (shader_->ID == 0) {
        std::cerr << "Invalid shader program ID" << std::endl;
        goto cleanup;
    }
    
    shader_->use();  // 直接调用，不检查返回值
    
    // 检查着色器程序是否有效
    glGetIntegerv(GL_CURRENT_PROGRAM, &program_id);
    // std::cout << "Current shader program: " << program_id << std::endl;
    
    if (program_id == 0) {
        std::cerr << "Failed to bind shader program" << std::endl;
        goto cleanup;
    }
    
    // 设置uniform
    shader_->setInt("y_texture", 0);
    shader_->setInt("u_texture", 1);
    shader_->setInt("v_texture", 2);
    
    // 4. 检查VAO状态
    if (vao_ == 0) {
        std::cerr << "VAO not initialized!" << std::endl;
        goto cleanup;
    }
    
    // 禁用不需要的状态
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    
    // 5. 绘制
    // std::cout << "Drawing with VAO: " << vao_ << std::endl;
    glBindVertexArray(vao_);
    
    // 检查顶点数组状态
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao_binding);
    // std::cout << "Bound VAO: " << vao_binding << std::endl;
    
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // 检查OpenGL错误
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error: " << error << std::endl;
    }
    
cleanup:
    // 6. 解绑FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_width_, window_height_);
    
    // 7. 更新UI纹理 - 修改这部分
    if (ui_layer_ && frame) {
        // 一次性更新所有视频信息
        ui_layer_->UpdateVideoInfo(m_renderTexture, frame->width, frame->height);
        
        // 调试输出
        static int frame_count = 0;
        if (++frame_count % 30 == 0) { // 每30帧输出一次
            // std::cout << "Rendered frame " << frame_count 
            //           << " - Texture: " << m_renderTexture 
            //           << ", Size: " << frame->width << "x" << frame->height << std::endl;
        }
    }
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

    // 清理FBO
    deleteFramebuffer();
    
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
        
        // 通知UI层窗口大小已改变
        if (ui_layer_) {
            // UI层可能需要调整布局
        }
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
    } 
    else 
    {
        SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
        fullscreen_ = true;
    }
}

void OpenGLRenderer::createFramebuffer(int width, int height)
{
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_renderTexture);
    glBindTexture(GL_TEXTURE_2D, m_renderTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_renderTexture, 0);
    
    // 验证FBO完整性
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "FBO not complete! Status: " << status << std::endl;
    } else {
        std::cout << "FBO created successfully! Texture ID: " << m_renderTexture << std::endl;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLRenderer::deleteFramebuffer() 
{
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_renderTexture) {
        glDeleteTextures(1, &m_renderTexture);
        m_renderTexture = 0;
    }
}

void OpenGLRenderer::setOpenVideoCallback(const std::function<void(const std::string&)>& callback)
{
    open_video_callback_ = callback;
    if (ui_layer_) {
        ui_layer_->SetOpenVideoCallback(callback);
    }
}

void OpenGLRenderer::handleSDLEvent(const SDL_Event& event)
{
    if (ui_layer_) 
    {
        // 先让ImGui处理事件
        ImGui_ImplSDL2_ProcessEvent(&event);
        
        // 检查ImGui是否想要捕获事件
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
            return; // ImGui想要捕获事件，跳过应用程序的事件处理
        }
    }
    
    // 处理应用程序事件
    switch (event.type) {
        case SDL_QUIT:
            if (state_) state_->quit = true;
            break;
            
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                handleResize(event.window.data1, event.window.data2);
            }
            break;
            
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    if (state_) state_->quit = true;
                    break;
                case SDLK_f:
                    toggleFullscreen();
                    break;
                case SDLK_i:
                    if (ui_layer_) {
                        ui_layer_->SetVisible(!ui_layer_->IsVisible());
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

void OpenGLRenderer::renderUI()
{
    if (!ui_layer_) return;
    
    // 开始ImGui帧
    ui_layer_->BeginFrame();
    
    // 渲染UI内容
    ui_layer_->Render();
    
    // 结束ImGui帧并交换缓冲区
    ui_layer_->EndFrame();
    SDL_GL_SwapWindow(window_);
}
// 添加新的辅助函数
void OpenGLRenderer::renderVideoToFBO(const AVFrame* frame) {
    if (!frame) return;
    
    // 上传纹理数据
    glBindTexture(GL_TEXTURE_2D, y_texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                   frame->width, frame->height,
                   GL_RED, GL_UNSIGNED_BYTE, frame->data[0]);
    
    glBindTexture(GL_TEXTURE_2D, u_texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                   frame->width/2, frame->height/2,
                   GL_RED, GL_UNSIGNED_BYTE, frame->data[1]);
    
    glBindTexture(GL_TEXTURE_2D, v_texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                   frame->width/2, frame->height/2,
                   GL_RED, GL_UNSIGNED_BYTE, frame->data[2]);
    
    // 使用着色器渲染
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
    
    // 绘制矩形
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // 解绑纹理
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool OpenGLRenderer::initForUIOnly() {
    // 创建SDL窗口
    window_ = SDL_CreateWindow(
        "FFmpeg Video Player",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    
    if (!window_) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // 设置OpenGL属性
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    
    // 创建OpenGL上下文
    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // 加载OpenGL函数
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }
    
    // 设置视口
    int width, height;
    SDL_GetWindowSize(window_, &width, &height);
    window_width_ = width;  // 修复：更新窗口尺寸成员变量
    window_height_ = height; // 修复：更新窗口尺寸成员变量
    glViewport(0, 0, width, height);
    
    // 启用深度测试
    glEnable(GL_DEPTH_TEST);
    
    // 初始化UI
    if (!ui_layer_->Init(window_, gl_context_)) {
        std::cerr << "Failed to initialize UI layer" << std::endl;
        return false;
    }
    
    ui_layer_->SetPlayerState(state_); // 修复：使用正确的成员变量名
    
    return true;
}

bool OpenGLRenderer::updateForNewVideo(int video_width, int video_height, AVPixelFormat pix_fmt) {
    // 清理之前的视频相关资源
    clearVideoResources();
    
    // 更新视频参数
    video_width_ = video_width;
    video_height_ = video_height;
    pix_fmt_ = pix_fmt;
    
    // 重新创建视频相关资源
    return createVideoResources(video_width, video_height, pix_fmt);
}

void OpenGLRenderer::clearVideoResources() {
    // 只清理视频相关的OpenGL资源，保留窗口和上下文
    if (y_texture_) { glDeleteTextures(1, &y_texture_); y_texture_ = 0; }
    if (u_texture_) { glDeleteTextures(1, &u_texture_); u_texture_ = 0; }
    if (v_texture_) { glDeleteTextures(1, &v_texture_); v_texture_ = 0; }
    
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (ebo_) { glDeleteBuffers(1, &ebo_); ebo_ = 0; }
    
    if (shader_) { delete shader_; shader_ = nullptr; }
    
    deleteFramebuffer();
    
    if (sws_ctx_) { sws_freeContext(sws_ctx_); sws_ctx_ = nullptr; }
}

bool OpenGLRenderer::createVideoResources(int width, int height, AVPixelFormat pix_fmt) {
    // 创建纹理
    createTextures(width, height);
    
    // 设置顶点数据
    setupVertexData();
    
    // 创建着色器
    shader_ = new Shader(
        "shaders/yuv_vertex.glsl",
        "shaders/yuv_fragment.glsl"
    );
    
    // 创建FBO
    createFramebuffer(width, height);
    
    if (!shader_ || shader_->ID == 0) {
        std::cerr << "Failed to create shader program" << std::endl;
        return false;
    }
    
    // 如果需要格式转换，创建sws上下文
    if (pix_fmt != AV_PIX_FMT_YUV420P) {
        sws_ctx_ = sws_getContext(
            width, height, pix_fmt,
            width, height, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        
        if (!sws_ctx_) {
            std::cerr << "Failed to create sws context" << std::endl;
            return false;
        }
    }
    
    return true;
}