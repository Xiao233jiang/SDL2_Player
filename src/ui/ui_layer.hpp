#pragma once

#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl2.h>
#include <imgui/imgui_impl_opengl3.h>
#include <functional>
#include <string>
#include <memory>

#include "gui_manager.hpp"
#include "panels/video_panel.hpp"
#include "panels/control_panel.hpp"

class PlayerState;
class OpenGLRenderer;

class UiLayer {
public:
    UiLayer(OpenGLRenderer* renderer);
    ~UiLayer();
    
    bool Init(SDL_Window* window, SDL_GLContext gl_context);
    void BeginFrame();
    void Render();
    void EndFrame();
    void Cleanup();
    
    void SetPlayerState(PlayerState* state);
    void SetOpenVideoCallback(const std::function<void(const std::string&)>& callback);
    
    GuiManager& GetGuiManager() { return m_guiManager; }
    
    void SetVisible(bool visible) { m_visible = visible; }
    bool IsVisible() const { return m_visible; }
    
    void SetVideoSize(int width, int height);
    void SetVideoTexture(GLuint texture);
    void UpdateVideoInfo(GLuint texture, int width, int height); // 新增：一次性更新
    void ClearVideoInfo(); // 新增：清除视频信息

private:
    void RegisterPanels();
    void SetupCleanStyle();
    void CreateMainLayout();
    void RenderMenuBar();
    void RenderDialogs();
    
    SDL_Window* m_window = nullptr;
    OpenGLRenderer* m_renderer = nullptr;
    PlayerState* m_playerState = nullptr;
    GuiManager m_guiManager;
    
    // 面板组件
    std::unique_ptr<VideoPanel> m_videoPanel;
    std::unique_ptr<ControlPanel> m_controlPanel;
    
    std::function<void(const std::string&)> m_openVideoCallback;
    bool m_visible = true;
    bool m_firstFrame = true;
};