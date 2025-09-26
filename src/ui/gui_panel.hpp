#pragma once

class PlayerApp;
class PlayerState;

class GuiPanel {
public:
    virtual ~GuiPanel() = default;
    
    virtual void Render() = 0;
    
    void SetApplication(PlayerApp* app) { m_app = app; }
    void SetPlayerState(PlayerState* state) { m_playerState = state; }
    
protected:
    PlayerApp* m_app = nullptr;
    PlayerState* m_playerState = nullptr;
};