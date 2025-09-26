// main_panel.hpp
#pragma once

#include "../gui_panel.hpp"
#include <functional>
#include <string>
#include <cstring> // 添加这个头文件用于 strncpy

class MainPanel : public GuiPanel {
public:
    void Render() override;
    
    void SetOpenVideoCallback(const std::function<void(const std::string&)>& callback) {
        m_openVideoCallback = callback;
    }
    
private:
    void RenderVideoControls();
    void RenderFileDialog();
    
    std::function<void(const std::string&)> m_openVideoCallback;
};