#include "file_dialog.hpp"
#include <windows.h>
#include <commdlg.h>
#include <codecvt>
#include <locale>
#include <iostream>

namespace {
    const char* GetVideoFileFilter() {
        static const char* video_filter = 
            "Video Files\0*.mp4;*.avi;*.mkv;*.mov;*.wmv;*.flv;*.m4v;*.3gp;*.webm;*.ts;*.mts;*.m2ts;*.mpg;*.mpeg;*.vob\0"
            "MP4 Files\0*.mp4;*.m4v\0"
            "AVI Files\0*.avi\0"
            "MKV Files\0*.mkv\0"
            "MOV Files\0*.mov;*.qt\0"
            "WMV Files\0*.wmv;*.asf\0"
            "MPEG Files\0*.mpg;*.mpeg;*.m2v;*.vob\0"
            "FLV Files\0*.flv;*.f4v\0"
            "WebM Files\0*.webm\0"
            "All Files\0*.*\0"
            "\0";
        return video_filter;
    }
}

std::string FileDialog::OpenFile(const std::string& title, const std::string& filter, const std::string& defaultPath) {
    OPENFILENAMEA ofn;
    char szFile[MAX_PATH] = {0};

    if (!defaultPath.empty()) {
        strncpy_s(szFile, defaultPath.c_str(), MAX_PATH - 1);
    }

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    
    // 使用辅助函数提供的过滤器
    ofn.lpstrFilter = filter.empty() ? GetVideoFileFilter() : filter.c_str();
    
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.lpstrTitle = title.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return std::string(szFile);
    }

    DWORD error = CommDlgExtendedError();
    if (error != 0) {
        std::cerr << "File dialog error code: " << error << std::endl;
    }

    return "";
}

std::string FileDialog::SaveFile(const std::string& title, const std::string& filter, 
                                const std::string& defaultPath, const std::string& defaultExt) {
    OPENFILENAMEA ofn;
    char szFile[MAX_PATH] = {0};

    if (!defaultPath.empty()) {
        strncpy_s(szFile, defaultPath.c_str(), MAX_PATH - 1);
    }

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.lpstrTitle = title.c_str();
    ofn.lpstrDefExt = defaultExt.empty() ? nullptr : defaultExt.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_EXPLORER;

    if (GetSaveFileNameA(&ofn) == TRUE) {
        return std::string(szFile);
    }

    return "";
}

std::string FileDialog::WideStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::wstring FileDialog::StringToWideString(const std::string& str) {
    if (str.empty()) return std::wstring();
    
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}