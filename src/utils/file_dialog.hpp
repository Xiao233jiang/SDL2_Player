#pragma once

#include <string>
#include <vector>

class FileDialog {
public:
    // 打开文件对话框 - 修复默认过滤器参数
    static std::string OpenFile(
        const std::string& title = "Open Video File",
        const std::string& filter = "", // 留空，让函数内部使用正确的格式
        const std::string& defaultPath = ""
    );

    // 保存文件对话框（如果需要）
    static std::string SaveFile(
        const std::string& title = "Save File",
        const std::string& filter = "", // 留空
        const std::string& defaultPath = "",
        const std::string& defaultExt = ""
    );

private:
    static std::string WideStringToString(const std::wstring& wstr);
    static std::wstring StringToWideString(const std::string& str);
};