#include "Chrome/EditorChrome.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cmath>

#include "imgui.h"

namespace
{
    constexpr float ButtonWidth = 46.0f;
    constexpr float Pi = 3.14159265358979323846f;
    constexpr float BrandIconLeftPadding = 8.0f;
    constexpr float BrandIconSize = 22.0f;
    constexpr float BrandIconMenuSpacing = 10.0f;
    constexpr float MenuStartX = BrandIconLeftPadding + BrandIconSize + BrandIconMenuSpacing;
    constexpr float MenuSpacingX = 2.0f;

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

    void DrawBrandIcon(const ImVec2& ViewportPosition)
    {
        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        if (DrawList == nullptr)
        {
            return;
        }

        constexpr ImU32 BrandColor = IM_COL32(41, 103, 255, 255);
        const ImU32 BackgroundColor = ImGui::GetColorU32(ImGuiCol_WindowBg);

        const ImVec2 Center(ViewportPosition.x + BrandIconLeftPadding + BrandIconSize * 0.5f,
                            ViewportPosition.y + FEditorChrome::TitleBarHeight * 0.5f);
        const float OuterRadius = BrandIconSize * 0.5f;
        const float InnerRadius = OuterRadius * 0.62f;

        ImVec2 Octagon[8];
        for (int32 Index = 0; Index < 8; ++Index)
        {
            const float Angle =
                Pi * 0.125f + (Pi * 0.25f * static_cast<float>(Index));
            Octagon[Index] = ImVec2(Center.x + std::cos(Angle) * OuterRadius,
                                    Center.y + std::sin(Angle) * OuterRadius);
        }
        DrawList->AddConvexPolyFilled(Octagon, 8, BrandColor);
        DrawList->AddCircleFilled(Center, InnerRadius, BackgroundColor, 32);

        const ImVec2 Direction(0.60f, -0.80f);
        const ImVec2 Perpendicular(0.80f, 0.60f);
        const ImVec2 Tip(Center.x + Direction.x * OuterRadius * 0.92f,
                         Center.y + Direction.y * OuterRadius * 0.92f);
        const ImVec2 Tail(Center.x - Direction.x * OuterRadius * 1.08f,
                          Center.y - Direction.y * OuterRadius * 1.08f);
        const ImVec2 Left(Center.x - Perpendicular.x * OuterRadius * 0.22f,
                          Center.y - Perpendicular.y * OuterRadius * 0.22f);
        const ImVec2 Right(Center.x + Perpendicular.x * OuterRadius * 0.22f,
                           Center.y + Perpendicular.y * OuterRadius * 0.22f);
        const ImVec2 Needle[4] = {Tip, Right, Tail, Left};
        DrawList->AddConvexPolyFilled(Needle, 4, BrandColor);

        const ImVec2 CutoutBase(Center.x - Direction.x * OuterRadius * 0.12f,
                                Center.y - Direction.y * OuterRadius * 0.12f);
        const ImVec2 CutoutTip(CutoutBase.x - Direction.x * OuterRadius * 0.26f,
                               CutoutBase.y - Direction.y * OuterRadius * 0.26f);
        const ImVec2 CutoutLeft(CutoutBase.x - Perpendicular.x * OuterRadius * 0.10f,
                                CutoutBase.y - Perpendicular.y * OuterRadius * 0.10f);
        const ImVec2 CutoutRight(CutoutBase.x + Perpendicular.x * OuterRadius * 0.10f,
                                 CutoutBase.y + Perpendicular.y * OuterRadius * 0.10f);
        const ImVec2 Cutout[3] = {CutoutTip, CutoutRight, CutoutLeft};
        DrawList->AddConvexPolyFilled(Cutout, 3, BackgroundColor);
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
        DrawBrandIcon(Viewport->Pos);

        const float ButtonStartX = Viewport->Size.x - (ButtonWidth * 3.0f);
        const ImVec2 ButtonSize(ButtonWidth, FEditorChrome::TitleBarHeight);
        const float MenuCursorY =
            (FEditorChrome::TitleBarHeight - ImGui::GetFrameHeight()) * 0.5f;

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

            AddInteractiveRect(InteractiveRects, Viewport->Pos);

            const ImVec2 RootItemMin = ImGui::GetItemRectMin();
            const ImVec2 RootItemMax = ImGui::GetItemRectMax();

            if (bAnyMenuPopupOpen && !bPopupOpen && ImGui::IsItemHovered())
            {
                ImGui::OpenPopup(PopupId.c_str());
            }

            ImGui::SetNextWindowPos(
                ImVec2(RootItemMin.x, Viewport->Pos.y + FEditorChrome::TitleBarHeight),
                ImGuiCond_Appearing);
            if (ImGui::BeginPopup(PopupId.c_str()))
            {
                DrawMenuItems(Menu.Items);
                ImGui::EndPopup();
            }

            MenuCursorX = RootItemMax.x - Viewport->Pos.x + MenuSpacingX;
        }

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

    Host->SetTitleBarMetrics(static_cast<int32>(FEditorChrome::TitleBarHeight),
                             InteractiveRects);
}
