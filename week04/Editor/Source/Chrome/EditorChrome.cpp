#include "Chrome/EditorChrome.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "imgui.h"

namespace
{
    constexpr float TitleBarHeight = 36.0f;
    constexpr float ButtonWidth = 46.0f;
    constexpr float TitlePaddingX = 12.0f;

    FEditorChromeRect MakeClientRect(const ImVec2& Min, const ImVec2& Max,
                                     const ImVec2& ViewportPosition)
    {
        FEditorChromeRect Rect;
        Rect.Left = static_cast<int32>(Min.x - ViewportPosition.x);
        Rect.Top = static_cast<int32>(Min.y - ViewportPosition.y);
        Rect.Right = static_cast<int32>(Max.x - ViewportPosition.x);
        Rect.Bottom = static_cast<int32>(Max.y - ViewportPosition.y);
        return Rect;
    }

    std::string WideToUtf8(const wchar_t* InText)
    {
        if (InText == nullptr || InText[0] == L'\0')
        {
            return {};
        }

        const int32 RequiredSize =
            WideCharToMultiByte(CP_UTF8, 0, InText, -1, nullptr, 0, nullptr, nullptr);
        if (RequiredSize <= 1)
        {
            return {};
        }

        std::string Converted(static_cast<size_t>(RequiredSize), '\0');
        WideCharToMultiByte(CP_UTF8, 0, InText, -1, Converted.data(), RequiredSize, nullptr,
                            nullptr);
        Converted.pop_back();
        return Converted;
    }
} // namespace

void FEditorChrome::Draw()
{
    if (Host == nullptr)
    {
        return;
    }

    ImGuiViewport* Viewport = ImGui::GetMainViewport();
    if (Viewport == nullptr)
    {
        return;
    }

    TArray<FEditorChromeRect> InteractiveRects;
    InteractiveRects.reserve(3);

    ImGui::SetNextWindowPos(Viewport->Pos);
    ImGui::SetNextWindowSize(ImVec2(Viewport->Size.x, TitleBarHeight));
    ImGuiWindowFlags WindowFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(37, 37, 38, 255));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(66, 66, 70, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(88, 88, 92, 255));

    if (ImGui::Begin("##EditorChrome", nullptr, WindowFlags))
    {
        const std::string TitleText = WideToUtf8(Host->GetWindowTitle());
        if (!TitleText.empty())
        {
            ImGui::SetCursorPos(ImVec2(TitlePaddingX,
                                       (TitleBarHeight - ImGui::GetTextLineHeight()) * 0.5f));
            ImGui::TextUnformatted(TitleText.c_str());
        }

        const float ButtonStartX = Viewport->Size.x - (ButtonWidth * 3.0f);
        const ImVec2 ButtonSize(ButtonWidth, TitleBarHeight);

        ImGui::SetCursorPos(ImVec2(ButtonStartX, 0.0f));
        if (ImGui::Button("-##EditorChromeMinimize", ButtonSize))
        {
            Host->MinimizeWindow();
        }
        InteractiveRects.push_back(
            MakeClientRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), Viewport->Pos));

        ImGui::SameLine(0.0f, 0.0f);
        const char* ToggleLabel =
            Host->IsWindowMaximized() ? "o##EditorChromeRestore" : "[]##EditorChromeMaximize";
        if (ImGui::Button(ToggleLabel, ButtonSize))
        {
            Host->ToggleMaximizeWindow();
        }
        InteractiveRects.push_back(
            MakeClientRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), Viewport->Pos));

        ImGui::SameLine(0.0f, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(200, 80, 80, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(170, 55, 55, 255));
        if (ImGui::Button("X##EditorChromeClose", ButtonSize))
        {
            Host->CloseWindow();
        }
        ImGui::PopStyleColor(2);
        InteractiveRects.push_back(
            MakeClientRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), Viewport->Pos));
    }
    ImGui::End();

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(5);

    Host->SetTitleBarMetrics(static_cast<int32>(TitleBarHeight), InteractiveRects);
}
