#include "gui_manager.hpp"
#include "player_app.hpp"
#include "player_core/player_state.hpp"

void GuiManager::AddPanel(const std::string& name, std::unique_ptr<GuiPanel> panel) {
    m_panels[name] = std::move(panel);
}

void GuiManager::RemovePanel(const std::string& name) {
    m_panels.erase(name);
}

void GuiManager::RenderPanel(const std::string& name) {
    auto it = m_panels.find(name);
    if (it != m_panels.end() && it->second) {
        it->second->Render();
    }
}

void GuiManager::RenderAllPanels() {
    for (auto& pair : m_panels) {
        if (pair.second) {
            pair.second->Render();
        }
    }
}

GuiPanel* GuiManager::GetPanel(const std::string& name) {
    auto it = m_panels.find(name);
    if (it != m_panels.end()) {
        return it->second.get();
    }
    return nullptr;
}

void GuiManager::SetApplication(PlayerApp* app) {
    for (auto& pair : m_panels) {
        if (pair.second) {
            pair.second->SetApplication(app);
        }
    }
}

void GuiManager::SetPlayerState(PlayerState* state) {
    for (auto& pair : m_panels) {
        if (pair.second) {
            pair.second->SetPlayerState(state);
        }
    }
}

void GuiManager::Clear() {
    m_panels.clear();
}