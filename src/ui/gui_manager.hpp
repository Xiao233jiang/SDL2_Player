#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include "gui_panel.hpp"

class PlayerApp;
class PlayerState;

class GuiManager {
public:
    void AddPanel(const std::string& name, std::unique_ptr<GuiPanel> panel);
    void RemovePanel(const std::string& name);
    
    void RenderPanel(const std::string& name);
    void RenderAllPanels();
    
    GuiPanel* GetPanel(const std::string& name);
    
    void SetApplication(PlayerApp* app);
    void SetPlayerState(PlayerState* state);
    
    void Clear();

private:
    std::unordered_map<std::string, std::unique_ptr<GuiPanel>> m_panels;
};