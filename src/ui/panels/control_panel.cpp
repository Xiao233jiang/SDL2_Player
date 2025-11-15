#include "control_panel.hpp"
#include "../../player_core/player_state.hpp"
#include "../../utils/file_dialog.hpp"

#include <SDL2/SDL.h>      // 用于 SDL_Log
#include <cstdio>          // snprintf
#include <algorithm>       // std::min, std::clamp
#include <cmath>           // std::abs

ControlPanel::ControlPanel() {}

// 常量提取，便于调整
static constexpr float DEFAULT_SPACING = 8.0f;
static constexpr float DEFAULT_PROGRESS_H = 35.0f;
static constexpr float DEFAULT_BUTTON_H = 120.0f;
static constexpr float DEFAULT_STATUS_H = 25.0f;

/**
 * @brief 主渲染方法（调整各区域高度使不出现内部滚动）
 */
void ControlPanel::Render(const ImVec2& available_size)
{
    // 快速保护
    if (!m_playerState) { ImGui::Dummy(available_size); return; }

    // 内边距：在左右上下预留一些空间，避免组件贴边
    const float pad_x = std::clamp(available_size.x * 0.03f, 6.0f, 16.0f); // 左右内边距
    const float pad_y = std::clamp(available_size.y * 0.03f, 4.0f, 12.0f); // 上下内边距

    // 在顶部留出垂直间距
    ImGui::Dummy(ImVec2(0.0f, pad_y));
    // 将后续内容整体右移 pad_x（使用 Indent/Unindent 不会改变窗口边界）
    ImGui::Indent(pad_x);

    // 可用内部尺寸（传递给子组件）
    ImVec2 inner_size = ImVec2(std::max(0.0f, available_size.x - pad_x * 2.0f),
                               std::max(0.0f, available_size.y - pad_y * 2.0f));

    // 如果没有加载视频，显示无视频状态（引用 RenderNoVideoState）
    if (!m_playerState->fmt_ctx) {
        RenderNoVideoState(inner_size);
        ImGui::Unindent(pad_x);
        ImGui::Dummy(ImVec2(0.0f, pad_y)); // 底部留白
        return;
    }

    // 更紧凑的进度高度与状态栏高度（基于内部高度）
    const float progress_height = std::clamp(inner_size.y * 0.12f, 10.0f, 14.0f); // 更细
    const float status_height = std::clamp(inner_size.y * 0.06f, 14.0f, 22.0f);    // 极简状态行
    const float spacing = 6.0f;

    // 计算控制区高度（保证不出现内滚动）
    float control_height = inner_size.y - progress_height - status_height - spacing * 2.0f;
    if (control_height < 44.0f) control_height = 44.0f; // 最小可用空间

    // 渲染：进度条（占用固定高度并垂直居中）
    RenderProgressBar(ImVec2(inner_size.x, progress_height));
    ImGui::Dummy(ImVec2(0, spacing));   // 留出间隔

    // 渲染：控制区（单块、自适应布局，不用子区域滚动）
    RenderControlButtons(ImVec2(inner_size.x, control_height));
    ImGui::Dummy(ImVec2(0, spacing));   // 留出间隔

    // 渲染状态栏（尽量占用很小高度）
    RenderStatusBar(ImVec2(inner_size.x, status_height));

    // 恢复缩进并在底部留白，保证父布局不会产生滚动
    ImGui::Unindent(pad_x);
    ImGui::Dummy(ImVec2(0.0f, pad_y));
}

/**
 * @brief 无视频状态
 */
void ControlPanel::RenderNoVideoState(const ImVec2& size) 
{
    const float btn_width = 200.0f;
    const float btn_height = 50.0f;
    
    ImVec2 center = ImVec2(
        (size.x - btn_width) * 0.5f,
        (size.y - btn_height) * 0.5f
    );
    
    ImGui::SetCursorPos(center);
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    
    if (ImGui::Button("Open Video File", ImVec2(btn_width, btn_height))) {
        std::string file = FileDialog::OpenFile("Select Video File");
        if (!file.empty() && m_openVideoCallback) {
            m_openVideoCallback(file);
        }
    }
    
    ImGui::PopStyleVar(1);
    ImGui::PopStyleColor(1);
}

/**
 * @brief 进度条 - 保持悬浮时间显示，使用成员变量保存寻址状态
 */
void ControlPanel::RenderProgressBar(const ImVec2& size) 
{
    // 保护性检查
    if (!m_playerState) {
        ImGui::Dummy(size);
        return;
    }

    // 未加载视频时：更细且垂直居中显示占位进度条
    if (!m_playerState->fmt_ctx) {
        float slider_h = std::clamp(size.y * 0.18f, 8.0f, 14.0f);
        float offset_y = (size.y - slider_h) * 0.5f;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset_y);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
        ImGui::ProgressBar(0.0f, ImVec2(size.x, slider_h));
        ImGui::PopStyleVar();
        // 占用原始高度，避免后续控件错位
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset_y);
        return;
    }

    // 获取时长与当前时间（秒）
    int64_t duration_ts = m_playerState->fmt_ctx->duration;
    double total_seconds = (duration_ts > 0) ? (double)duration_ts / (double)AV_TIME_BASE : 0.0;
    double current_seconds = m_playerState->video_clock.get();
    float progress = (total_seconds > 0.0) ? std::clamp((float)(current_seconds / total_seconds), 0.0f, 1.0f) : 0.0f;

    // 轨道高度与垂直偏移（使轨道在给定区域内垂直居中）
    float slider_h = std::clamp(size.y * 0.18f, 8.0f, 14.0f);
    float offset_y = (size.y - slider_h) * 0.5f;

    // 预计算左右时间文本及宽度
    bool show_time = size.x > 300.0f && total_seconds > 0.0;
    char cur_text[32] = {0}, total_text[32] = {0};
    float left_text_w = 0.0f, right_text_w = 0.0f;
    if (show_time) {
        int cur_m = (int)(current_seconds / 60.0);
        int cur_s = (int)current_seconds % 60;
        int tot_m = (int)(total_seconds / 60.0);
        int tot_s = (int)total_seconds % 60;
        snprintf(cur_text, sizeof(cur_text), "%02d:%02d", cur_m, cur_s);
        snprintf(total_text, sizeof(total_text), "%02d:%02d", tot_m, tot_s);
        left_text_w = ImGui::CalcTextSize(cur_text).x;
        right_text_w = ImGui::CalcTextSize(total_text).x;
    }

    const float gap = 8.0f; // 文本与滑块间距
    float slider_width = size.x;
    if (show_time) {
        slider_width = std::max(40.0f, size.x - left_text_w - right_text_w - gap * 2.0f);
    }

    // 保存基准屏幕坐标并计算滑块位置
    ImVec2 base_screen = ImGui::GetCursorScreenPos();
    float y_top = base_screen.y + offset_y;
    float y_center = y_top + slider_h * 0.5f;

    // 左时间绘制（使用屏幕坐标保证垂直居中）
    float cursor_x = base_screen.x;
    if (show_time) {
        ImGui::SetCursorScreenPos(ImVec2(cursor_x, y_center - ImGui::GetTextLineHeight() * 0.5f));  
        ImGui::TextUnformatted(cur_text);
        cursor_x += left_text_w + gap;
    }

    // 在滑块处设置 InvisibleButton 以处理交互
    ImGui::SetCursorScreenPos(ImVec2(cursor_x, y_top));
    ImGui::InvisibleButton("##progress_invisible", ImVec2(slider_width, slider_h));
    ImVec2 rect_min = ImGui::GetItemRectMin();
    ImVec2 rect_max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 背景轨道
    ImU32 col_bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
    ImU32 col_fill = ImGui::GetColorU32(ImGuiCol_SliderGrab);
    float rounding = 2.0f;
    dl->AddRectFilled(rect_min, rect_max, col_bg, rounding);

    // 填充进度
    float filled_x = rect_min.x + (m_seeking ? (m_seek_pos * slider_width) : (progress * slider_width));
    dl->AddRectFilled(rect_min, ImVec2(filled_x, rect_max.y), col_fill, rounding);

    // 把手（小圆），垂直居中于轨道
    float handle_radius = std::max(4.0f, slider_h * 0.6f);
    ImU32 col_handle = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
    dl->AddCircleFilled(ImVec2(filled_x, y_center), handle_radius, col_handle);

    // 悬停预览：半透明填充 + 竖线 + tooltip
    if (ImGui::IsItemHovered()) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float hover_p = std::clamp((mouse.x - rect_min.x) / slider_width, 0.0f, 1.0f);
        double hover_seconds = hover_p * total_seconds;
        int hm = (int)(hover_seconds / 60), hs = (int)hover_seconds % 60;
        char tip[64];
        snprintf(tip, sizeof(tip), "%02d:%02d / %02d:%02d  (%.1f%%)",
                 hm, hs, (int)(current_seconds/60), (int)current_seconds%60, hover_p * 100.0f);
        ImGui::SetTooltip("%s", tip);

        float preview_x = rect_min.x + hover_p * slider_width;
        dl->AddRectFilled(ImVec2(rect_min.x, rect_min.y - 1),
                          ImVec2(preview_x, rect_max.y + 1),
                          IM_COL32(255, 255, 0, 28), 1.0f);
        dl->AddLine(ImVec2(preview_x, rect_min.y - 1),
                    ImVec2(preview_x, rect_max.y + 1),
                    IM_COL32(255, 255, 0, 160), 1.0f);
    }

    // 交互：拖拽更新 m_seek_pos，释放时提交 seek；点击也提交
    if (ImGui::IsItemActive()) {
        m_seeking = true;
        ImVec2 mouse = ImGui::GetIO().MousePos;
        m_seek_pos = std::clamp((mouse.x - rect_min.x) / slider_width, 0.0f, 1.0f);
    }
    if (m_seeking && !ImGui::IsItemActive()) {
        m_seeking = false;
        if (m_playerState && total_seconds > 0.0) m_playerState->doSeekAbsolute(m_seek_pos * total_seconds);
    }
    if (ImGui::IsItemClicked()) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float new_p = std::clamp((mouse.x - rect_min.x) / slider_width, 0.0f, 1.0f);
        if (m_playerState && total_seconds > 0.0) m_playerState->doSeekAbsolute(new_p * total_seconds);
    }

    // 右时间绘制：位于滑块右侧，垂直居中
    if (show_time) {
        float right_x = cursor_x + slider_width + gap;
        ImGui::SetCursorScreenPos(ImVec2(right_x, y_center - ImGui::GetTextLineHeight() * 0.5f));
        ImGui::TextUnformatted(total_text);
    }

    // 将内部光标移到原始区域底部，保证后续布局正常
    ImGui::SetCursorScreenPos(ImVec2(base_screen.x, base_screen.y + size.y));
}

/**
 * @brief 控制按钮 - 无内部滚动、紧凑自适应布局
 * 布局：三列（左 音量；中 seek+play；右 速度+打开文件）
 */
void ControlPanel::RenderControlButtons(const ImVec2& size)
{
    // 保护
    if (!m_playerState) {
        ImGui::Dummy(size);
        return;
    }

    // 缩小全局内边距以节省空间（函数局部修改）
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 3));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 4));

    // 使用 Table 三列
    if (ImGui::BeginTable("ControlsTable", 3, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableSetupColumn("Center", ImGuiTableColumnFlags_WidthStretch, 0.50f);
        ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableNextRow();

        // 左列：音量（水平紧凑）
        ImGui::TableSetColumnIndex(0);
        {
            float col_w = ImGui::GetColumnWidth();
            // 音量控件布局
            RenderVolumeControl(col_w, size);
        }

        // 中列：seek + play（水平居中，按钮按高度自适应）
        ImGui::TableSetColumnIndex(1);
        {
            float col_w = ImGui::GetColumnWidth();
            // 计算按钮尺寸：保证五个按钮能水平显示
            float item_spacing = ImGui::GetStyle().ItemSpacing.x;
            float btn_max_width = (col_w - item_spacing * 4.0f) / 5.0f;
            float btn_size = std::clamp(btn_max_width, 20.0f, 48.0f);
            // 更小屏幕使用更短文本
            ImVec2 bsize = ImVec2(btn_size, std::min(btn_size, size.y - 8.0f));

            // 居中：计算起始 X
            float total_w = 5.0f * bsize.x + 4.0f * item_spacing;
            float start_x = (col_w - total_w) * 0.5f;
            if (start_x > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + start_x);

            // 五个按钮：-10 -5 play +5 +10
            RenderSeekButtons(bsize.x);
        }

        // 右列：速度与打开文件（垂直紧凑）
        ImGui::TableSetColumnIndex(2);
        {
            float col_w = ImGui::GetColumnWidth();
            // 播放速度与文件打开
            RenderSpeedControl(col_w, size.y);
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar(2);
}

/**
 * @brief 状态栏 - 极简版
 */
void ControlPanel::RenderStatusBar(const ImVec2& size) 
{
    if (!m_playerState) return;
    
    ImGui::Separator();
    
    // 单行状态显示
    const char* status = m_playerState->fmt_ctx ? "||" : "|>";
    ImVec4 color = m_playerState->fmt_ctx ? 
        ImVec4(0.3f, 0.8f, 0.3f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    
    ImGui::TextColored(color, "%s", status);  // 播放状态（首行左）
    ImGui::SameLine();
    ImGui::Text("V:%d A:%d", 
                (int)m_playerState->video_frame_queue.size(),
                (int)m_playerState->audio_frame_queue.size());   // 帧队列（首行中）
    
    // 文件名（如果有空间）
    if (size.x > 350.0f && !m_playerState->filename.empty()) {
        ImGui::SameLine();
        std::string filename = m_playerState->filename;
        
        // 提取文件名
        size_t pos = filename.find_last_of("/\\");
        if (pos != std::string::npos) {
            filename = filename.substr(pos + 1);
        }
        
        // 截断长文件名
        if (filename.length() > 25) {
            filename = filename.substr(0, 22) + "...";
        }
        
        ImGui::Text("| %s", filename.c_str());
    }
}

/**
 * @brief 精简的 Seek 按钮组（保证单行显示）
 */
void ControlPanel::RenderSeekButtons(float button_size)
{
    // 保证按钮之间的间距足够小
    float spacing = ImGui::GetStyle().ItemSpacing.x * 0.6f;

    // 紧凑标签，保证不换行
    if (ImGui::Button("<<10", ImVec2(button_size, button_size))) if (m_playerState) m_playerState->doSeekRelative(-10.0);
    ImGui::SameLine(0, spacing);
    if (ImGui::Button("<<5", ImVec2(button_size, button_size))) if (m_playerState) m_playerState->doSeekRelative(-5.0);
    ImGui::SameLine(0, spacing);

    // Play 按钮略大
    RenderPlayButton(button_size * 1.2f);
    ImGui::SameLine(0, spacing);

    if (ImGui::Button("5>>", ImVec2(button_size, button_size))) if (m_playerState) m_playerState->doSeekRelative(5.0);
    ImGui::SameLine(0, spacing);
    if (ImGui::Button("10>>", ImVec2(button_size, button_size))) if (m_playerState) m_playerState->doSeekRelative(10.0);
}

/**
 * @brief 播放/暂停按钮 - 保持紧凑尺寸
 */
void ControlPanel::RenderPlayButton(float button_size)
{
    bool is_playing = m_playerState && m_playerState->fmt_ctx && !m_playerState->paused.load();
    const char* label = is_playing ? "||" : "|>";
    ImGui::PushStyleColor(ImGuiCol_Button, is_playing ? ImVec4(0.9f,0.5f,0.2f,1.0f) : ImVec4(0.2f,0.7f,0.3f,1.0f));
    if (ImGui::Button(label, ImVec2(button_size, std::min(button_size, 44.0f)))) {
        if (m_playerState && m_playerState->fmt_ctx) m_playerState->paused.store(!is_playing);
    }
    ImGui::PopStyleColor();
}

/**
 * @brief 音量控制 - 水平紧凑布局（小高度）
 */
void ControlPanel::RenderVolumeControl(float width, const ImVec2& size)
{
    ImGui::PushID("vol_ctrl");

    // 布局参数
    const float icon_w = 44.0f; // 更大按钮宽度
    const float icon_h = std::clamp(size.y * 0.65f, 28.0f, 44.0f); // 更大按钮高度
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float slider_h = std::clamp(size.y * 0.28f, 8.0f, 14.0f); // 滑条高度
    const float total_w = std::clamp(width * 0.85f, 120.0f, 320.0f);

    float slider_w = std::max(60.0f, total_w - icon_w - spacing - 36.0f);

    // 水平居中
    float cur_x = ImGui::GetCursorPosX();
    float start_x = cur_x + (width - total_w) * 0.5f;
    ImGui::SetCursorPosX(start_x);

    // 垂直居中整体区域
    float base_y = ImGui::GetCursorPosY();
    float max_h = std::max(icon_h, slider_h + 8.0f);
    float offset_y = (size.y - max_h) * 0.5f;
    if (offset_y > 0) ImGui::SetCursorPosY(base_y + offset_y);

    ImGui::BeginGroup();

    // 静音/音量按钮（更大更显眼）
    if (ImGui::Button(m_isMuted ? "静音" : "音量", ImVec2(icon_w, icon_h))) {
        m_isMuted = !m_isMuted;
        if (m_isMuted) m_volume = 0.0f; // 静音时滑条归零
    }
    ImGui::SameLine(0, spacing);

    // 滑条与文本整体垂直居中
    ImGui::BeginGroup();
    float slider_y = ImGui::GetCursorPosY() + (icon_h - slider_h) * 0.5f;
    ImGui::SetCursorPosY(slider_y);

    // 滑条
    ImVec2 slider_pos = ImGui::GetCursorScreenPos();
    float display_volume = m_isMuted ? 0.0f : m_volume; // 静音时显示为0
    ImGui::InvisibleButton("##vol_slider", ImVec2(slider_w, slider_h));
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 传递调节的音量
    if (m_playerState) 
    {
        m_playerState->volume = m_volume;
    }

    // 背景轨道
    ImU32 col_bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
    ImU32 col_fill = ImGui::GetColorU32(ImGuiCol_SliderGrab);
    float rounding = 2.0f;
    dl->AddRectFilled(slider_pos, ImVec2(slider_pos.x + slider_w, slider_pos.y + slider_h), col_bg, rounding);

    // 填充进度
    float filled_x = slider_pos.x + display_volume * slider_w;
    dl->AddRectFilled(slider_pos, ImVec2(filled_x, slider_pos.y + slider_h), col_fill, rounding);

    // 把手（小圆），垂直居中于轨道
    float handle_radius = std::max(4.0f, slider_h * 0.6f);
    ImU32 col_handle = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
    dl->AddCircleFilled(ImVec2(filled_x, slider_pos.y + slider_h * 0.5f), handle_radius, col_handle);

    // 交互：拖拽/点击
    if (ImGui::IsItemActive() || ImGui::IsItemClicked()) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float new_vol = std::clamp((mouse.x - slider_pos.x) / slider_w, 0.0f, 1.0f);
        m_volume = new_vol;
        if (m_volume > 0.001f) m_isMuted = false; // 拖动时自动取消静音
        if (m_playerState) {
            SDL_Log("Volume changed: %.0f%%", m_volume * 100.0f);
        }
    }

    // 悬停预览
    if (ImGui::IsItemHovered()) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float hover_vol = std::clamp((mouse.x - slider_pos.x) / slider_w, 0.0f, 1.0f);
        char tip[32];
        snprintf(tip, sizeof(tip), "音量: %d%%", (int)(hover_vol * 100.0f));
        ImGui::SetTooltip("%s", tip);
    }
    ImGui::EndGroup();

    // 百分比文本，水平对齐
    ImGui::SameLine(0, spacing + 14.0f); // 更大间距
    float text_y = slider_y + (slider_h - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorPosY(text_y);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", (int)std::round(display_volume * 100.0f));
    ImGui::TextUnformatted(buf);

    ImGui::EndGroup();

    ImGui::PopID();
}

/**
 * @brief 速度控制 - 增强自适应：在窄或矮空间使用按钮行代替下拉
 */
void ControlPanel::RenderSpeedControl(float width, float height) 
{
    // 速度选择：否则下拉
    ImGui::PushID("speed_ctrl");
        // 下拉占满宽度（但高度小）
        int idx = 3;
        static const char* speeds[] = {"0.25x","0.5x","0.75x","1.0x","1.25x","1.5x","2.0x"};
        static const float speed_vals[] = {0.25f,0.5f,0.75f,1.0f,1.25f,1.5f,2.0f};
        for (int i=0;i<7;i++) if (std::abs(m_playbackSpeed - speed_vals[i]) < 0.01f) { idx = i; break; }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##speed", &idx, speeds, 7)) m_playbackSpeed = speed_vals[idx];
    ImGui::PopID();

    // 打开文件：尽量小的高度并占满列宽
    ImGui::Spacing();
    ImVec2 file_btn = ImVec2(-1, std::min(30.0f, height * 0.36f));
    if (ImGui::Button("打开文件", file_btn)) {
        std::string f = FileDialog::OpenFile("Select Video File");
        if (!f.empty() && m_openVideoCallback) m_openVideoCallback(f);
    }
}