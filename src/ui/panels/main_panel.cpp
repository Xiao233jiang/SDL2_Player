#include "main_panel.hpp"
#include <imgui/imgui.h>
#include <iostream>
#include "../../player_core/player_state.hpp"

void MainPanel::Render() {
    // 这个面板现在主要用于调试信息
    if (!ImGui::Begin("调试信息", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }
    
    if (m_playerState) {
        ImGui::Text("播放状态: %s", m_playerState->quit.load() ? "已停止" : "播放中");
        
        ImGui::Separator();
        
        // 队列状态
        ImGui::Text("音频包队列: %d/%d", 
                   m_playerState->audio_packet_queue.size(), MAX_AUDIO_PACKETS);
        ImGui::Text("视频包队列: %d/%d", 
                   m_playerState->video_packet_queue.size(), MAX_VIDEO_PACKETS);
        ImGui::Text("音频帧队列: %d/%d", 
                   m_playerState->audio_frame_queue.size(), MAX_AUDIO_FRAMES);
        ImGui::Text("视频帧队列: %d/%d", 
                   m_playerState->video_frame_queue.size(), MAX_VIDEO_FRAMES);
        
        ImGui::Separator();
        
        // 时钟信息
        ImGui::Text("视频时钟: %.3f s", m_playerState->video_clock.get());
        ImGui::Text("音频时钟: %.3f s", m_playerState->audio_clock.get());
        
        // 流信息
        if (m_playerState->audio_stream >= 0) {
            ImGui::Text("音频流: %d", m_playerState->audio_stream);
        }
        if (m_playerState->video_stream >= 0) {
            ImGui::Text("视频流: %d", m_playerState->video_stream);
        }
        
        // 进度信息
        if (m_playerState->fmt_ctx && m_playerState->fmt_ctx->duration != AV_NOPTS_VALUE) {
            double total_seconds = m_playerState->fmt_ctx->duration / AV_TIME_BASE;
            double current_seconds = m_playerState->video_clock.get();
            float progress = (total_seconds > 0) ? static_cast<float>(current_seconds / total_seconds) : 0.0f;
            
            ImGui::Separator();
            ImGui::Text("播放进度: %.1f / %.1f 秒", current_seconds, total_seconds);
            ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
        }
        
    } else {
        ImGui::Text("播放器未初始化");
    }
    
    ImGui::End();
}

void MainPanel::RenderVideoControls() {
    // 这个方法现在可以简化或移除
    if (m_playerState) {
        if (ImGui::Button(m_playerState->quit.load() ? "播放" : "暂停")) {
            m_playerState->quit = !m_playerState->quit.load();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("停止")) {
            m_playerState->quit = true;
        }
    }
}

void MainPanel::RenderFileDialog() {
    // 简化的文件对话框
    if (ImGui::BeginPopupModal("Open File", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char filename[512] = "";
        
        ImGui::Text("请输入文件路径:");
        ImGui::InputText("##filepath", filename, sizeof(filename));
        
        if (ImGui::Button("打开")) {
            if (m_openVideoCallback && filename[0] != '\0') {
                m_openVideoCallback(filename);
            }
            filename[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("取消")) {
            filename[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}