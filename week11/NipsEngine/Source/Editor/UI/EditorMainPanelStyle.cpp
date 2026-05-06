#include "Editor/UI/EditorMainPanel.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

void FEditorMainPanel::ConfigureImGuiStyle()
{
    ImGuiStyle& Style = ImGui::GetStyle();
    Style.WindowRounding = 6.0f;
    Style.FrameRounding = 6.0f;
    Style.GrabRounding = 6.0f;
    Style.PopupRounding = 6.0f;
    Style.TabRounding = 6.0f;
    Style.ScrollbarRounding = 6.0f;
    Style.WindowBorderSize = 1.0f;
    Style.FrameBorderSize = 0.0f;
    Style.Colors[ImGuiCol_Text] = ImVec4(0.93f, 0.94f, 0.96f, 1.0f);
    Style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.56f, 0.62f, 1.0f);
    Style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.09f, 0.11f, 1.0f);
    Style.Colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.11f, 0.14f, 1.0f);
    Style.Colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.11f, 0.13f, 0.98f);
    Style.Colors[ImGuiCol_Border] = ImVec4(0.20f, 0.23f, 0.27f, 1.0f);
    Style.Colors[ImGuiCol_FrameBg] = ImVec4(0.17f, 0.19f, 0.22f, 1.0f);
    Style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.21f, 0.24f, 0.29f, 1.0f);
    Style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.16f, 0.30f, 0.53f, 1.0f);
    Style.Colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.10f, 0.12f, 1.0f);
    Style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.12f, 0.15f, 1.0f);
    Style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.11f, 0.12f, 0.15f, 1.0f);
    Style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.22f, 0.26f, 0.95f);
    Style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.29f, 0.35f, 1.0f);
    Style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.16f, 0.30f, 0.53f, 1.0f);
    Style.Colors[ImGuiCol_Header] = ImVec4(0.19f, 0.22f, 0.27f, 1.0f);
    Style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.23f, 0.28f, 0.35f, 1.0f);
    Style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.16f, 0.30f, 0.53f, 1.0f);
    Style.Colors[ImGuiCol_Tab] = ImVec4(0.14f, 0.16f, 0.20f, 1.0f);
    Style.Colors[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.26f, 0.33f, 1.0f);
    Style.Colors[ImGuiCol_TabActive] = ImVec4(0.16f, 0.30f, 0.53f, 1.0f);
    Style.Colors[ImGuiCol_CheckMark] = ImVec4(0.32f, 0.61f, 0.93f, 1.0f);
    Style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.32f, 0.61f, 0.93f, 1.0f);
    Style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.39f, 0.69f, 0.97f, 1.0f);
}

void FEditorMainPanel::LoadEditorFonts()
{
    ImGuiIO& IO = ImGui::GetIO();
    ImFontGlyphRangesBuilder KoreanBuilder;
    KoreanBuilder.AddRanges(IO.Fonts->GetGlyphRangesKorean());
    KoreanBuilder.AddRanges(IO.Fonts->GetGlyphRangesDefault());
    KoreanBuilder.BuildRanges(&FontGlyphRanges);

    IO.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/malgun.ttf", 16.0f, nullptr, FontGlyphRanges.Data);

    ImFontConfig IconConfig;
    IconConfig.MergeMode = true;
    IconConfig.PixelSnapH = true;

    static const ImWchar IconRanges[] = {
        0x23F8, 0x23F8,
        0x25A0, 0x25A0,
        0x25B6, 0x25B6,
        0,
    };
    IO.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/seguisym.ttf", 16.0f, &IconConfig, IconRanges);

    ImFontConfig FallbackConfig;
    FallbackConfig.MergeMode = true;
    IO.Fonts->AddFontFromFileTTF(
        "C:/Windows/Fonts/msyh.ttc",
        16.0f,
        &FallbackConfig,
        IO.Fonts->GetGlyphRangesChineseFull());
}
