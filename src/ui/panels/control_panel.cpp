#include "control_panel.hpp"
#include "../../player_core/player_state.hpp"
#include "../../utils/file_dialog.hpp"

ControlPanel::ControlPanel() {}

void ControlPanel::Render(const ImVec2& available_size) {
    bool has_video = (m_playerState && m_playerState->fmt_ctx != nullptr);
    
    if (!has_video) {
        RenderNoVideoControls(available_size);
    } else {
        RenderPlaybackControls(available_size);
        ImGui::Spacing();
        RenderProgressBar();
        ImGui::Spacing();
        RenderVolumeAndSpeedControls();
        ImGui::Spacing();
        RenderStatusInfo();
    }
}

void ControlPanel::RenderNoVideoControls(const ImVec2& available_size) {
    float button_width = 200.0f;
    float button_height = 50.0f;
    float center_x = std::max(0.0f, (available_size.x - button_width) * 0.5f);
    float center_y = std::max(0.0f, (available_size.y - button_height) * 0.5f);
    
    ImGui::SetCursorPos(ImVec2(center_x, center_y));
    
    // 美化按钮
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.3f, 0.7f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    
    if (ImGui::Button("Open Video File", ImVec2(button_width, button_height))) {
        std::string selected_file = FileDialog::OpenFile("Select Video File");
        if (!selected_file.empty() && m_openVideoCallback) {
            m_openVideoCallback(selected_file);
        }
    }
    
    ImGui::PopStyleVar(1);
    ImGui::PopStyleColor(3);
    
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Select a video file to play");
    }
}

void ControlPanel::RenderPlaybackControls(const ImVec2& available_size) {
    const float button_height = 45.0f;
    const float button_width = 55.0f;
    const float icon_button_width = 50.0f;
    const float play_button_width = 65.0f;
    const float spacing = 8.0f;
    
    // 计算总宽度用于居中
    float total_width = icon_button_width * 4 + play_button_width + button_width * 2 + spacing * 6;
    float start_x = std::max(0.0f, (available_size.x - total_width) * 0.5f);
    
    ImGui::SetCursorPosX(start_x);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, 4.0f));
    
    // 快退按钮
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.45f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.25f, 1.0f));
    
    if (ImGui::Button("<<10", ImVec2(icon_button_width, button_height))) {
        if (m_playerState) m_playerState->doSeekRelative(-10.0);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rewind 10 seconds");
    
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    
    // 后退按钮
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.45f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.25f, 1.0f));
    
    if (ImGui::Button("<5", ImVec2(icon_button_width, button_height))) {
        if (m_playerState) m_playerState->doSeekRelative(-5.0);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rewind 5 seconds");
    
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    
    // 播放/暂停按钮
    bool is_playing = m_playerState && !m_playerState->quit.load() && m_playerState->fmt_ctx;
    const char* play_text = is_playing ? "PAUSE" : "PLAY";
    
    ImVec4 play_color = is_playing ? 
        ImVec4(0.8f, 0.4f, 0.2f, 1.0f) : ImVec4(0.2f, 0.7f, 0.3f, 1.0f);
    ImVec4 play_hover = is_playing ? 
        ImVec4(0.9f, 0.5f, 0.3f, 1.0f) : ImVec4(0.3f, 0.8f, 0.4f, 1.0f);
    ImVec4 play_active = is_playing ? 
        ImVec4(0.7f, 0.3f, 0.1f, 1.0f) : ImVec4(0.1f, 0.6f, 0.2f, 1.0f);
    
    ImGui::PushStyleColor(ImGuiCol_Button, play_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, play_hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, play_active);
    
    if (ImGui::Button(play_text, ImVec2(play_button_width, button_height))) {
        printf("%s button clicked - implement proper play/pause logic\n", play_text);
    }
    
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    
    // 停止按钮
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
    
    if (ImGui::Button("STOP", ImVec2(button_width, button_height))) {
        printf("Stop button clicked\n");
    }
    
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    
    // 前进和快进按钮
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.45f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.25f, 1.0f));
    
    if (ImGui::Button("5>", ImVec2(icon_button_width, button_height))) {
        if (m_playerState) m_playerState->doSeekRelative(5.0);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Forward 5 seconds");
    
    ImGui::SameLine();
    if (ImGui::Button("10>>", ImVec2(icon_button_width, button_height))) {
        if (m_playerState) m_playerState->doSeekRelative(10.0);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Forward 10 seconds");
    
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    
    // 打开文件按钮
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.7f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.5f, 1.0f));
    
    if (ImGui::Button("OPEN", ImVec2(button_width, button_height))) {
        std::string selected_file = FileDialog::OpenFile("Select Video File");
        if (!selected_file.empty() && m_openVideoCallback) {
            m_openVideoCallback(selected_file);
        }
    }
    
    ImGui::PopStyleColor(3);
    
    // 修复：只Pop 2次，匹配开始时的2次Push
    ImGui::PopStyleVar(2);  // 只有这一行，删除重复的行
}

void ControlPanel::RenderVolumeAndSpeedControls() {
    if (ImGui::BeginTable("Controls", 2, ImGuiTableFlags_SizingStretchProp)) {
        // 音量控制
        ImGui::TableNextColumn();
        ImGui::Text("Volume:");
        ImGui::SameLine();
        
        const char* mute_text = m_isMuted ? "[MUTE]" : "[SOUND]";
        ImVec4 mute_color = m_isMuted ? 
            ImVec4(0.8f, 0.3f, 0.3f, 1.0f) : ImVec4(0.3f, 0.6f, 0.3f, 1.0f);
        
        ImGui::PushStyleColor(ImGuiCol_Button, mute_color);
        if (ImGui::Button(mute_text, ImVec2(70, 25))) {
            m_isMuted = !m_isMuted;
        }
        ImGui::PopStyleColor(1);
        
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        
        char volume_text[32];
        snprintf(volume_text, sizeof(volume_text), "%.0f%%", m_volume * 100.0f);
        ImGui::SliderFloat("##volume", &m_volume, 0.0f, 1.0f, volume_text);
        
        // 播放速度控制
        ImGui::TableNextColumn();
        ImGui::Text("Speed:");
        ImGui::SameLine();
        
        const float speeds[] = {0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f};
        const char* speed_labels[] = {"0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "2.0x"};
        
        for (int i = 0; i < 6; i++) {
            if (i > 0) ImGui::SameLine();
            
            bool is_current = (fabs(m_playbackSpeed - speeds[i]) < 0.01f);
            if (is_current) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
            }
            
            if (ImGui::Button(speed_labels[i], ImVec2(40, 20))) {
                m_playbackSpeed = speeds[i];
            }
            
            if (is_current) {
                ImGui::PopStyleColor(1);
            }
        }
        
        ImGui::EndTable();
    }
}

void ControlPanel::RenderProgressBar() {
    if (!m_playerState || !m_playerState->fmt_ctx) {
        // 无媒体时的进度条
        ImVec2 available = ImGui::GetContentRegionAvail();
        float progress_width = available.x * 0.90f;
        float center_x = (available.x - progress_width) * 0.5f;
        
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + center_x);
        
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::ProgressBar(0.0f, ImVec2(progress_width, 40), "No media loaded");
        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(1);
        return;
    }
    
    int64_t duration = m_playerState->fmt_ctx->duration;
    if (duration == AV_NOPTS_VALUE) {
        ImVec2 available = ImGui::GetContentRegionAvail();
        float progress_width = available.x * 0.90f;
        float center_x = (available.x - progress_width) * 0.5f;
        
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + center_x);
        
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::ProgressBar(0.0f, ImVec2(progress_width, 40), "Unknown duration");
        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(1);
        return;
    }
    
    double total_seconds = duration / (double)AV_TIME_BASE;
    double current_seconds = m_playerState->video_clock.get();
    
    float progress = 0.0f;
    if (total_seconds > 0.0) {
        progress = (float)(current_seconds / total_seconds);
        progress = std::max(0.0f, std::min(1.0f, progress));
    }
    
    // 创建水平布局：时间 + 进度条 + 时间
    ImVec2 available = ImGui::GetContentRegionAvail();
    
    // 左侧当前时间
    char current_time_text[16];
    int curr_min = (int)(current_seconds / 60);
    int curr_sec = (int)(current_seconds) % 60;
    snprintf(current_time_text, sizeof(current_time_text), "%02d:%02d", curr_min, curr_sec);
    ImVec2 current_time_size = ImGui::CalcTextSize(current_time_text);
    
    // 右侧总时间
    char total_time_text[16];
    int total_min = (int)(total_seconds / 60);
    int total_sec = (int)(total_seconds) % 60;
    snprintf(total_time_text, sizeof(total_time_text), "%02d:%02d", total_min, total_sec);
    ImVec2 total_time_size = ImGui::CalcTextSize(total_time_text);
    
    // 计算进度条宽度
    float spacing = 15.0f;
    float progress_width = available.x - current_time_size.x - total_time_size.x - spacing * 2;
    progress_width = std::max(progress_width, 300.0f);
    
    // 渲染当前时间
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "%s", current_time_text);
    ImGui::SameLine(0, spacing);
    
    // 进度条样式
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.22f, 0.22f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.2f, 0.7f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.1f, 0.6f, 0.9f, 1.0f));
    
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 24.0f);
    
    // ✅ 修复的拖拽逻辑
    static bool is_seeking = false;
    static float seek_target = 0.0f;
    
    ImGui::SetNextItemWidth(progress_width);
    float slider_value = progress;
    
    // 如果正在seeking，使用目标值而不是当前值
    if (is_seeking) {
        slider_value = seek_target;
    }
    
    bool slider_changed = ImGui::SliderFloat("##seekbar", &slider_value, 0.0f, 1.0f, "", 
                                           ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput);
    
    bool is_active = ImGui::IsItemActive();
    bool was_active = ImGui::IsItemDeactivated();
    
    if (is_active && !is_seeking) {
        // 开始拖拽
        is_seeking = true;
        seek_target = slider_value;
        printf("=== Started seeking at %.2f%% ===\n", seek_target * 100.0f);
    }
    
    if (is_seeking) {
        if (is_active) {
            // 拖拽中 - 更新目标但不执行seek
            seek_target = slider_value;
            
            // 显示预览时间
            double preview_seconds = seek_target * total_seconds;
            int preview_min = (int)(preview_seconds / 60);
            int preview_sec = (int)(preview_seconds) % 60;
            
            char preview_text[64];
            snprintf(preview_text, sizeof(preview_text), "Seeking to: %02d:%02d (%.1f%%)", 
                    preview_min, preview_sec, seek_target * 100.0f);
            ImGui::SetTooltip("%s", preview_text);
        }
        
        if (was_active || !is_active) {
            // 结束拖拽 - 执行seek
            is_seeking = false;
            
            double target_seconds = seek_target * total_seconds;
            printf("=== Executing seek to %.2fs (%.1f%%) ===\n", target_seconds, seek_target * 100.0f);
            
            if (m_playerState) {
                m_playerState->doSeekAbsolute(target_seconds);
            }
        }
    }
    
    // 悬停显示预览（非拖拽状态）
    if (ImGui::IsItemHovered() && !is_seeking) {
        ImVec2 mouse_pos = ImGui::GetMousePos();
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        cursor_pos.y -= 45; // 调整到进度条位置
        
        float hover_progress = (mouse_pos.x - cursor_pos.x) / progress_width;
        hover_progress = std::max(0.0f, std::min(1.0f, hover_progress));
        
        double hover_seconds = hover_progress * total_seconds;
        int hover_min = (int)(hover_seconds / 60);
        int hover_sec = (int)(hover_seconds) % 60;
        
        char hover_text[64];
        snprintf(hover_text, sizeof(hover_text), "Jump to: %02d:%02d (%.1f%%)", 
                hover_min, hover_sec, hover_progress * 100.0f);
        ImGui::SetTooltip("%s", hover_text);
    }
    
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(5);
    
    ImGui::SameLine(0, spacing);
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "%s", total_time_text);
}

void ControlPanel::RenderStatusInfo() {
    if (!m_playerState) return;
    
    ImGui::Separator();
    
    if (ImGui::BeginTable("StatusTable", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Video Queue");
        ImGui::TableSetupColumn("Audio Queue");
        ImGui::TableSetupColumn("File");
        ImGui::TableHeadersRow();
        
        ImGui::TableNextRow();
        
        // 状态
        ImGui::TableNextColumn();
        const char* status_text = m_playerState->fmt_ctx ? "[PLAYING]" : "[STOPPED]";
        ImVec4 status_color = m_playerState->fmt_ctx ? 
            ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        ImGui::TextColored(status_color, "%s", status_text);
        
        // 队列信息
        ImGui::TableNextColumn();
        ImGui::Text("V: %d", (int)m_playerState->video_frame_queue.size());
        
        ImGui::TableNextColumn();
        ImGui::Text("A: %d", (int)m_playerState->audio_frame_queue.size());
        
        // 文件信息
        ImGui::TableNextColumn();
        if (!m_playerState->filename.empty()) {
            std::string filename = m_playerState->filename;
            size_t pos = filename.find_last_of("/\\");
            if (pos != std::string::npos) {
                filename = filename.substr(pos + 1);
            }
            if (filename.length() > 15) {
                filename = filename.substr(0, 12) + "...";
            }
            ImGui::Text("%s", filename.c_str());
        } else {
            ImGui::Text("No file");
        }
        
        ImGui::EndTable();
    }
}