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
    void SetOpenVideoCallback(const std::function<void(const std::string&)>& callback) { m_openVideoCallback = callback; }
    
    void Render(const ImVec2& available_size);

private:
    void RenderNoVideoControls(const ImVec2& available_size);
    void RenderPlaybackControls(const ImVec2& available_size);
    void RenderProgressBar();
    void RenderVolumeAndSpeedControls();
    void RenderStatusInfo();
    
    PlayerState* m_playerState = nullptr;
    std::function<void(const std::string&)> m_openVideoCallback;
    
    // UI状态
    float m_volume = 1.0f;
    bool m_isMuted = false;
    float m_playbackSpeed = 1.0f;
};