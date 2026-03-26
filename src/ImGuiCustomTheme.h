#pragma once

#include <rlImGui.h>
#include <imgui.h>
#include <string>

inline void ImCustomTheme(const std::string& path = "assets/Font/Roboto-Regular.ttf")
{
	// Load a custom font
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Clear();
	io.Fonts->AddFontFromFileTTF(path.c_str(), 20.0f);
	rlImGuiReloadFonts();

	// Apply a simple dark-gray theme to frame backgrounds and rounding
	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_FrameBg] = ImColor(0.22f, 0.22f, 0.22f, 1.0f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImColor(0.20f, 0.20f, 0.20f, 1.0f);
	style.Colors[ImGuiCol_FrameBgActive] = ImColor(0.30f, 0.30f, 0.30f, 1.0f);
	style.WindowRounding = 10.0f;
}