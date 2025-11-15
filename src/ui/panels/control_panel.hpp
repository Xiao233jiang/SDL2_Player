#pragma once

#include <imgui/imgui.h>
#include <functional>
#include <string>

class PlayerState;

class ControlPanel {
public:
    ControlPanel();
    ~ControlPanel() = default;
    
    void SetPlayerState(PlayerState* state) { m_playerState = state; }
    void SetOpenVideoCallback(const std::function<void(const std::string&)>& callback) { 
        m_openVideoCallback = callback; 
    }
    
    void Render(const ImVec2& available_size);

private:
    // 只保留核心渲染方法
    void RenderNoVideoState(const ImVec2& size);
    void RenderProgressBar(const ImVec2& size);
    void RenderControlButtons(const ImVec2& size);
    void RenderStatusBar(const ImVec2& size);
    
    // 合并辅助方法
    void RenderSeekButtons(float button_size);
    void RenderPlayButton(float button_size);
    void RenderVolumeControl(float width, const ImVec2& size);
    void RenderSpeedControl(float width, float height);
    
    PlayerState* m_playerState = nullptr;
    std::function<void(const std::string&)> m_openVideoCallback;
    
    // UI状态
    float m_volume = 1.0f;
    bool m_isMuted = false;
    float m_playbackSpeed = 1.0f;
    
    // 将寻址状态移出函数，便于管理与测试
    bool m_seeking = false;
    float m_seek_pos = 0.0f;
};