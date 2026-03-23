#include "Chrome/EditorChrome.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "imgui.h"

namespace
{
    constexpr float ButtonWidth = 46.0f;
    constexpr float MenuStartX = 8.0f;
    constexpr float MenuSpacingX = 2.0f;
    constexpr float TitlePaddingX = 12.0f;
    constexpr float TitleSpacingX = 10.0f;
    constexpr float TitleRightPaddingX = 12.0f;

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

    void AddInteractiveRect(TArray<FEditorChromeRect>& OutRects, const ImVec2& ViewportPosition)
    {
        OutRects.push_back(
            MakeClientRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ViewportPosition));
    }

    void DrawMenuItems(const TArray<FEditorChromeMenuItem>& Items)
    {
        // chrome renderer는 이미 해석된 메뉴 트리를 그대로 순회하며 ImGui popup 메뉴를 그립니다.
        for (const FEditorChromeMenuItem& Item : Items)
        {
            const std::string ItemLabel = WideToUtf8(Item.Label.c_str());
            switch (Item.Type)
            {
            case EEditorChromeMenuItemType::Separator:
                ImGui::Separator();
                break;

            case EEditorChromeMenuItemType::SubMenu:
                if (ImGui::BeginMenu(ItemLabel.c_str(), Item.bEnabled))
                {
                    DrawMenuItems(Item.Children);
                    ImGui::EndMenu();
                }
                break;

            case EEditorChromeMenuItemType::Action:
            default:
            {
                const char* ShortcutText =
                    Item.ShortcutLabel.empty() ? nullptr : Item.ShortcutLabel.c_str();
                const bool bActivated =
                    ImGui::MenuItem(ItemLabel.c_str(), ShortcutText,
                                    Item.bCheckable ? Item.bChecked : false, Item.bEnabled);
                if (bActivated)
                {
                    if (Item.OnTriggered)
                    {
                        Item.OnTriggered();
                    }
                    ImGui::CloseCurrentPopup();
                }
                break;
            }
            }
        }
    }

    std::string BuildMenuPopupId(size_t Index)
    {
        return "##EditorChromeMenuPopup" + std::to_string(Index);
    }

    void DrawTitleText(const ImVec2& ViewportPosition, float TitleLeft, float TitleRight,
                       const std::string& TitleText)
    {
        if (TitleText.empty() || TitleRight <= TitleLeft)
        {
            return;
        }

        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        if (DrawList == nullptr)
        {
            return;
        }

        const ImVec2 TextSize = ImGui::CalcTextSize(TitleText.c_str());
        const ImVec2 TextPosition(TitleLeft,
                                  ViewportPosition.y +
                                      (FEditorChrome::TitleBarHeight - TextSize.y) * 0.5f);

        DrawList->PushClipRect(ImVec2(TitleLeft, ViewportPosition.y),
                               ImVec2(TitleRight, ViewportPosition.y + FEditorChrome::TitleBarHeight),
                               true);
        DrawList->AddText(TextPosition, ImGui::GetColorU32(ImGuiCol_Text), TitleText.c_str());
        DrawList->PopClipRect();
    }
} // namespace

void FEditorChrome::Draw(const TArray<FEditorChromeMenu>& Menus)
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
    InteractiveRects.reserve(3 + Menus.size());

    ImGui::SetNextWindowPos(Viewport->Pos);
    ImGui::SetNextWindowSize(ImVec2(Viewport->Size.x, FEditorChrome::TitleBarHeight));
    ImGuiWindowFlags WindowFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(MenuSpacingX, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(37, 37, 38, 255));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(45, 45, 48, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(66, 66, 70, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(88, 88, 92, 255));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(66, 66, 70, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(88, 88, 92, 255));

    if (ImGui::Begin("##EditorChrome", nullptr, WindowFlags))
    {
        // 한 줄 안에 메뉴, 제목, 시스템 버튼을 모두 배치하기 위한 기준 좌표입니다.
        const float ButtonStartX = Viewport->Size.x - (ButtonWidth * 3.0f);
        const ImVec2 ButtonSize(ButtonWidth, FEditorChrome::TitleBarHeight);
        const float MenuCursorY =
            (FEditorChrome::TitleBarHeight - ImGui::GetFrameHeight()) * 0.5f;
        float MenuEndX = Viewport->Pos.x + TitlePaddingX;

        bool bAnyMenuPopupOpen = false;
        for (size_t MenuIndex = 0; MenuIndex < Menus.size(); ++MenuIndex)
        {
            if (ImGui::IsPopupOpen(BuildMenuPopupId(MenuIndex).c_str(), ImGuiPopupFlags_None))
            {
                bAnyMenuPopupOpen = true;
                break;
            }
        }

        float MenuCursorX = MenuStartX;
        for (size_t MenuIndex = 0; MenuIndex < Menus.size(); ++MenuIndex)
        {
            const FEditorChromeMenu& Menu = Menus[MenuIndex];
            const std::string PopupId = BuildMenuPopupId(MenuIndex);
            const std::string MenuLabel = WideToUtf8(Menu.Label.c_str());
            const bool bPopupOpen = ImGui::IsPopupOpen(PopupId.c_str(), ImGuiPopupFlags_None);

            ImGui::SetCursorPos(ImVec2(MenuCursorX, MenuCursorY));
            if (bPopupOpen)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(66, 66, 70, 255));
            }

            if (ImGui::Button(MenuLabel.c_str()))
            {
                ImGui::OpenPopup(PopupId.c_str());
            }

            if (bPopupOpen)
            {
                ImGui::PopStyleColor();
            }

            // 메뉴 루트 버튼은 창 드래그가 시작되면 안 되므로 interactive rect로 따로 수집합니다.
            AddInteractiveRect(InteractiveRects, Viewport->Pos);

            const ImVec2 RootItemMin = ImGui::GetItemRectMin();
            const ImVec2 RootItemMax = ImGui::GetItemRectMax();

            if (bAnyMenuPopupOpen && !bPopupOpen && ImGui::IsItemHovered())
            {
                ImGui::OpenPopup(PopupId.c_str());
            }

            ImGui::SetNextWindowPos(ImVec2(RootItemMin.x, Viewport->Pos.y + FEditorChrome::TitleBarHeight),
                                    ImGuiCond_Appearing);
            if (ImGui::BeginPopup(PopupId.c_str()))
            {
                DrawMenuItems(Menu.Items);
                ImGui::EndPopup();
            }

            MenuCursorX = RootItemMax.x - Viewport->Pos.x + MenuSpacingX;
            MenuEndX = RootItemMax.x;
        }

        const std::string TitleText = WideToUtf8(Host->GetWindowTitle());
        const float TitleLeft =
            (MenuEndX + TitleSpacingX > Viewport->Pos.x + TitlePaddingX)
                ? (MenuEndX + TitleSpacingX)
                : (Viewport->Pos.x + TitlePaddingX);
        const float TitleRight = Viewport->Pos.x + ButtonStartX - TitleRightPaddingX;
        // 제목은 메뉴와 창 버튼 사이 남는 공간에만 클리핑해서 그립니다.
        DrawTitleText(Viewport->Pos, TitleLeft, TitleRight, TitleText);

        ImGui::SetCursorPos(ImVec2(ButtonStartX, 0.0f));
        if (ImGui::Button("-##EditorChromeMinimize", ButtonSize))
        {
            Host->MinimizeWindow();
        }
        AddInteractiveRect(InteractiveRects, Viewport->Pos);

        ImGui::SameLine(0.0f, 0.0f);
        const char* ToggleLabel =
            Host->IsWindowMaximized() ? "o##EditorChromeRestore" : "[]##EditorChromeMaximize";
        if (ImGui::Button(ToggleLabel, ButtonSize))
        {
            Host->ToggleMaximizeWindow();
        }
        AddInteractiveRect(InteractiveRects, Viewport->Pos);

        ImGui::SameLine(0.0f, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(200, 80, 80, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(170, 55, 55, 255));
        if (ImGui::Button("X##EditorChromeClose", ButtonSize))
        {
            Host->CloseWindow();
        }
        ImGui::PopStyleColor(2);
        AddInteractiveRect(InteractiveRects, Viewport->Pos);
    }
    ImGui::End();

    ImGui::PopStyleColor(7);
    ImGui::PopStyleVar(5);

    // 이번 프레임에 실제로 클릭 가능한 영역을 Win32 쪽으로 넘겨 caption drag와 충돌하지 않게 합니다.
    Host->SetTitleBarMetrics(static_cast<int32>(FEditorChrome::TitleBarHeight),
                             InteractiveRects);
}
