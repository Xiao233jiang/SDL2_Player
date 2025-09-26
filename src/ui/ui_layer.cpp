#include "ui_layer.hpp"
#include "../play/opengl_renderer.hpp"
#include "player_core/player_state.hpp"
#include "panels/main_panel.hpp"
#include "../utils/file_dialog.hpp"

UiLayer::UiLayer(OpenGLRenderer* renderer) 
    : m_renderer(renderer),
      m_videoPanel(std::make_unique<VideoPanel>()),
      m_controlPanel(std::make_unique<ControlPanel>()) {
}

UiLayer::~UiLayer() {
    Cleanup();
}

bool UiLayer::Init(SDL_Window* window, SDL_GLContext gl_context) {
    m_window = window;
    
    // 设置ImGui上下文
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // 设置字体
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        "assets/fonts/kaiu.ttf",
        24.0f,
        nullptr,
        io.Fonts->GetGlyphRangesChineseFull()
    );
    IM_ASSERT(font != nullptr);
    
    SetupCleanStyle();
    
    // 初始化ImGui后端
    if (!ImGui_ImplSDL2_InitForOpenGL(m_window, gl_context)) {
        return false;
    }
    
    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        return false;
    }
    
    RegisterPanels();
    
    return true;
}

void UiLayer::SetupCleanStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // 简洁的暗色主题
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.95f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.95f);
    colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
    
    // 设置样式参数
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
}

void UiLayer::BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void UiLayer::Render() {
    CreateMainLayout();
    m_guiManager.RenderAllPanels();
}

void UiLayer::CreateMainLayout() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (!viewport) return;
    
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    
    ImGuiWindowFlags window_flags = 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoCollapse | 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | 
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    if (ImGui::Begin("Media Player", nullptr, window_flags)) {
        RenderMenuBar();
        
        ImVec2 available = ImGui::GetContentRegionAvail();
        if (available.x > 0 && available.y > 0) {
            float control_height = 120.0f;
            float video_height = available.y - control_height;
            
            if (video_height < 50.0f) {
                video_height = available.y * 0.7f;
                control_height = available.y - video_height;
            }
            
            // 视频区域
            if (ImGui::BeginChild("VideoArea", ImVec2(0, video_height), false, ImGuiWindowFlags_NoScrollbar)) {
                ImVec2 video_area = ImGui::GetContentRegionAvail();
                m_videoPanel->Render(video_area);
            }
            ImGui::EndChild();
            
            ImGui::Separator();
            
            // 控制区域
            if (ImGui::BeginChild("ControlArea", ImVec2(0, control_height), false, ImGuiWindowFlags_NoScrollbar)) {
                ImVec2 control_area = ImGui::GetContentRegionAvail();
                m_controlPanel->Render(control_area);
            }
            ImGui::EndChild();
        }
    }
    
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void UiLayer::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Video", "Ctrl+O")) {
                std::string selected_file = FileDialog::OpenFile("Select Video File");
                if (!selected_file.empty() && m_openVideoCallback) {
                    m_openVideoCallback(selected_file);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                if (m_playerState) {
                    m_playerState->quit = true;
                }
                SDL_Event quit_event;
                quit_event.type = SDL_QUIT;
                SDL_PushEvent(&quit_event);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                ImGui::OpenPopup("AboutDialog");
            }
            ImGui::EndMenu();
        }
        
        ImGui::EndMenuBar();
    }
    
    RenderDialogs();
}

void UiLayer::RenderDialogs() {
    // 关于对话框
    if (ImGui::BeginPopupModal("AboutDialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("FFmpeg Media Player");
        ImGui::Separator();
        ImGui::Text("Version: 1.0.0");
        ImGui::Text("Built with: FFmpeg, SDL2, OpenGL, ImGui");
        
        ImGui::Separator();
        
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void UiLayer::EndFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void UiLayer::Cleanup() {
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }
}

void UiLayer::SetPlayerState(PlayerState* state) {
    m_playerState = state;
    m_guiManager.SetPlayerState(state);
    m_videoPanel->SetPlayerState(state);
    m_controlPanel->SetPlayerState(state);
}

void UiLayer::SetOpenVideoCallback(const std::function<void(const std::string&)>& callback) {
    m_openVideoCallback = callback;
    m_videoPanel->SetOpenVideoCallback(callback);
    m_controlPanel->SetOpenVideoCallback(callback);
    
    MainPanel* mainPanel = dynamic_cast<MainPanel*>(m_guiManager.GetPanel("MainPanel"));
    if (mainPanel) {
        mainPanel->SetOpenVideoCallback(callback);
    }
}

void UiLayer::SetVideoSize(int width, int height) {
    if (m_videoPanel) {
        m_videoPanel->SetVideoSize(width, height);
        printf("UiLayer: Set video size: %dx%d\n", width, height);
    }
}

void UiLayer::SetVideoTexture(GLuint texture) {
    if (m_videoPanel) {
        m_videoPanel->SetVideoTexture(texture);
        printf("UiLayer: Set video texture: %u\n", texture);
    }
}

void UiLayer::UpdateVideoInfo(GLuint texture, int width, int height) {
    if (m_videoPanel) {
        m_videoPanel->SetVideoInfo(texture, width, height);
        printf("UiLayer: Updated video info - Texture: %u, Size: %dx%d\n", texture, width, height);
    }
}

void UiLayer::ClearVideoInfo() {
    if (m_videoPanel) {
        m_videoPanel->ClearVideo();
        printf("UiLayer: Cleared video info\n");
    }
}

void UiLayer::RegisterPanels() {
    m_guiManager.AddPanel("MainPanel", std::make_unique<MainPanel>());
}

