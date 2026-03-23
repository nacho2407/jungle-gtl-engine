#include "Chrome/EditorChrome.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "imgui.h"

namespace
{
    constexpr float ButtonWidth = 46.0f;
    constexpr float TitlePaddingX = 12.0f;

    FEditorChromeRect MakeClientRect(const ImVec2& Min, const ImVec2& Max,
                                     const ImVec2& ViewportPosition)
    {
        // ImGui viewport 좌표를 Win32 client 좌표로 바꿔 hit-test에 전달합니다.
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

    // 이 창은 실제 타이틀바 UI만 담당하고, 드래그/리사이즈 판정은 Win32 hit-test가 처리합니다.
    ImGui::SetNextWindowPos(Viewport->Pos);
    ImGui::SetNextWindowSize(ImVec2(Viewport->Size.x, FEditorChrome::TitleBarHeight));
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
                                       (FEditorChrome::TitleBarHeight -
                                        ImGui::GetTextLineHeight()) *
                                           0.5f));
            ImGui::TextUnformatted(TitleText.c_str());
        }

        const float ButtonStartX = Viewport->Size.x - (ButtonWidth * 3.0f);
        const ImVec2 ButtonSize(ButtonWidth, FEditorChrome::TitleBarHeight);

        ImGui::SetCursorPos(ImVec2(ButtonStartX, 0.0f));
        if (ImGui::Button("-##EditorChromeMinimize", ButtonSize))
        {
            Host->MinimizeWindow();
        }
        // 버튼 사각형은 드래그 가능 영역에서 제외해야 클릭 시 창이 움직이지 않습니다.
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

    // 매 프레임 최신 버튼 위치를 플랫폼 계층에 넘겨 hit-test 결과를 동기화합니다.
    Host->SetTitleBarMetrics(static_cast<int32>(FEditorChrome::TitleBarHeight),
                             InteractiveRects);
}
