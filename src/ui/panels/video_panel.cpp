#include "video_panel.hpp"
#include "../../player_core/player_state.hpp"
#include "../../utils/file_dialog.hpp"

VideoPanel::VideoPanel() {}

void VideoPanel::SetVideoInfo(GLuint texture, int width, int height) {
    m_videoTexture = texture;
    m_videoWidth = width;
    m_videoHeight = height;
    m_hasVideoData = (texture != 0 && width > 0 && height > 0);
    
    if (m_hasVideoData) {
        printf("VideoPanel: Set video info - Texture: %u, Size: %dx%d\n", texture, width, height);
    }
}

void VideoPanel::SetVideoTexture(GLuint texture) {
    m_videoTexture = texture;
    m_hasVideoData = (texture != 0 && m_videoWidth > 0 && m_videoHeight > 0);
    
    if (m_hasVideoData) {
        printf("VideoPanel: Updated texture: %u\n", texture);
    }
}

void VideoPanel::SetVideoSize(int width, int height) {
    m_videoWidth = width;
    m_videoHeight = height;
    m_hasVideoData = (m_videoTexture != 0 && width > 0 && height > 0);
    
    printf("VideoPanel: Set video size: %dx%d\n", width, height);
}

void VideoPanel::ClearVideo() {
    m_videoTexture = 0;
    m_videoWidth = 0;
    m_videoHeight = 0;
    m_hasVideoData = false;
    printf("VideoPanel: Cleared video data\n");
}

bool VideoPanel::HasValidVideo() const {
    return m_hasVideoData && m_videoTexture != 0 && m_videoWidth > 0 && m_videoHeight > 0;
}

void VideoPanel::Render(const ImVec2& available_size) {
    // 检查是否有视频数据或正在加载
    bool is_loading = (m_playerState && m_playerState->loading.load());
    bool has_valid_video = HasValidVideo();
    
    if (has_valid_video) {
        RenderVideo(available_size);
    } else {
        RenderPlaceholder(available_size);
    }
}

void VideoPanel::RenderVideo(const ImVec2& available_size) {
    if (!HasValidVideo()) {
        RenderPlaceholder(available_size);
        return;
    }
    
    // 计算保持宽高比的视频尺寸
    float video_aspect = (float)m_videoWidth / (float)m_videoHeight;
    float area_aspect = available_size.x / available_size.y;
    
    ImVec2 video_size;
    ImVec2 offset;
    
    if (area_aspect > video_aspect) {
        video_size.y = available_size.y;
        video_size.x = video_size.y * video_aspect;
        offset.x = (available_size.x - video_size.x) * 0.5f;
        offset.y = 0;
    } else {
        video_size.x = available_size.x;
        video_size.y = video_size.x / video_aspect;
        offset.x = 0;
        offset.y = (available_size.y - video_size.y) * 0.5f;
    }
    
    // 绘制黑色背景
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
    ImVec2 area_min = cursor_pos;
    ImVec2 area_max = ImVec2(cursor_pos.x + available_size.x, cursor_pos.y + available_size.y);
    
    draw_list->AddRectFilled(area_min, area_max, IM_COL32(0, 0, 0, 255));
    
    // 显示视频
    ImGui::SetCursorPos(offset);
    ImGui::Image((void*)(intptr_t)m_videoTexture, video_size);
    
    if (ImGui::IsItemClicked()) {
        printf("Video clicked - implement play/pause\n");
    }
    
    // 调试信息（可选）
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Video: %dx%d, Texture: %u", m_videoWidth, m_videoHeight, m_videoTexture);
    }
}

void VideoPanel::RenderPlaceholder(const ImVec2& available_size) {
    ImVec2 center = ImVec2(available_size.x * 0.5f, available_size.y * 0.5f);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
    
    ImVec2 area_min = cursor_pos;
    ImVec2 area_max = ImVec2(cursor_pos.x + available_size.x, cursor_pos.y + available_size.y);
    
    // 渐变背景
    draw_list->AddRectFilledMultiColor(
        area_min, area_max,
        IM_COL32(15, 15, 20, 255),   
        IM_COL32(25, 25, 35, 255),   
        IM_COL32(35, 35, 45, 255),   
        IM_COL32(20, 20, 30, 255)    
    );
    
    draw_list->AddRect(area_min, area_max, IM_COL32(60, 60, 80, 100), 2.0f);
    
    ImVec2 content_center = ImVec2(cursor_pos.x + center.x, cursor_pos.y + center.y);
    
    // 播放图标
    float icon_size = std::min(available_size.x, available_size.y) * 0.15f;
    icon_size = std::max(80.0f, std::min(icon_size, 120.0f));
    
    draw_list->AddCircleFilled(content_center, icon_size + 8, IM_COL32(40, 40, 50, 200));
    draw_list->AddCircleFilled(content_center, icon_size, IM_COL32(70, 130, 200, 180));
    
    // 播放三角形
    float triangle_size = icon_size * 0.4f;
    ImVec2 p1 = ImVec2(content_center.x - triangle_size * 0.3f, content_center.y - triangle_size);
    ImVec2 p2 = ImVec2(content_center.x - triangle_size * 0.3f, content_center.y + triangle_size);
    ImVec2 p3 = ImVec2(content_center.x + triangle_size * 0.8f, content_center.y);
    
    draw_list->AddTriangleFilled(p1, p2, p3, IM_COL32(255, 255, 255, 240));
    
    // 文字显示
    const char* main_text;
    const char* sub_text;
    
    bool is_loading = (m_playerState && m_playerState->loading.load());
    bool has_filename = (m_playerState && !m_playerState->filename.empty());
    
    if (is_loading) {
        main_text = "Loading Video...";
        sub_text = "Please wait while the video is being processed";
    } else if (has_filename) {
        main_text = "Video Processing";
        sub_text = "Video file loaded, waiting for first frame...";
    } else {
        main_text = "No Video Loaded";
        sub_text = "Click to open video file";
    }
    
    ImVec2 main_text_size = ImGui::CalcTextSize(main_text);
    ImVec2 main_text_pos = ImVec2(
        content_center.x - main_text_size.x * 0.5f, 
        content_center.y + icon_size + 30
    );
    draw_list->AddText(main_text_pos, IM_COL32(200, 200, 220, 255), main_text);
    
    ImVec2 sub_text_size = ImGui::CalcTextSize(sub_text);
    ImVec2 sub_text_pos = ImVec2(
        content_center.x - sub_text_size.x * 0.5f, 
        main_text_pos.y + main_text_size.y + 15
    );
    draw_list->AddText(sub_text_pos, IM_COL32(150, 150, 170, 200), sub_text);
    
    // 点击区域（只在没有加载时允许点击）
    if (!is_loading) {
        ImGui::SetCursorScreenPos(cursor_pos);
        ImGui::InvisibleButton("video_placeholder", available_size);
        
        if (ImGui::IsItemClicked()) {
            std::string selected_file = FileDialog::OpenFile("Select Video File");
            if (!selected_file.empty() && m_openVideoCallback) {
                m_openVideoCallback(selected_file);
            }
        }
        
        if (ImGui::IsItemHovered()) {
            draw_list->AddRect(area_min, area_max, IM_COL32(100, 150, 200, 150), 2.0f, 0, 3.0f);
            ImGui::SetTooltip("Click to open video file");
        }
    }
}