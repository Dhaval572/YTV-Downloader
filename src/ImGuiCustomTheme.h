#pragma once

#include <rlImGui.h>
#include <imgui.h>
#include <filesystem>

#if defined(__linux__)
#include <cstdlib>
#endif

inline std::filesystem::path asset_path(const std::string& file)
{
#if defined(__linux__)
    if (const char* appdir = std::getenv("APPDIR"))
    {
        return std::filesystem::path(appdir) / "usr/share/ytv_downloader/assets" / file;
    }
#endif
    return std::filesystem::path("assets") / file;
}

inline void ImCustomTheme()
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    auto font = asset_path("Font/Roboto-Regular.ttf");
    io.Fonts->AddFontFromFileTTF(font.string().c_str(), 20.0f);

    rlImGuiReloadFonts();

    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_FrameBg]        = ImColor(0.22f, 0.22f, 0.22f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImColor(0.20f, 0.20f, 0.20f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive]  = ImColor(0.30f, 0.30f, 0.30f, 1.0f);
    style.WindowRounding = 10.0f;
}