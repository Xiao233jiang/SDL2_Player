#pragma once

#include <imgui/imgui.h>
#include <glad/glad.h>
#include <functional>
#include <string>

class PlayerState;

class VideoPanel {
public:
    VideoPanel();
    ~VideoPanel() = default;
    
    void SetPlayerState(PlayerState* state) { m_playerState = state; }
    void SetOpenVideoCallback(const std::function<void(const std::string&)>& callback) { m_openVideoCallback = callback; }
    
    // 修改：一次性设置所有视频信息
    void SetVideoInfo(GLuint texture, int width, int height);
    void SetVideoTexture(GLuint texture); // 单独设置纹理
    void SetVideoSize(int width, int height); // 单独设置尺寸
    void ClearVideo(); // 清除视频信息
    
    void Render(const ImVec2& available_size);

private:
    void RenderVideo(const ImVec2& available_size);
    void RenderPlaceholder(const ImVec2& available_size);
    bool HasValidVideo() const; // 检查是否有有效视频
    
    PlayerState* m_playerState = nullptr;
    std::function<void(const std::string&)> m_openVideoCallback;
    
    GLuint m_videoTexture = 0;
    int m_videoWidth = 0;
    int m_videoHeight = 0;
    bool m_hasVideoData = false; // 新增：明确标记是否有视频数据
};