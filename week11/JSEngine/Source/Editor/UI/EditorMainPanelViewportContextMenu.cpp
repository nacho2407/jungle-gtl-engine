#include "Editor/UI/EditorMainPanel.h"
#include "Editor/UI/EditorMainPanelPlacementHelpers.h"

#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Math/Utils.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cfloat>

void FEditorMainPanel::TickViewportContextMenu()
{
    if (!EditorEngine || !Window || ImGui::IsPopupOpen("##ViewportContextMenu"))
    {
        return;
    }

    if (Widgets.ContentBrowserWidget.IsVisible() && Widgets.ContentBrowserWidget.IsMouseOverBrowser())
    {
        ViewportContextMenuState.bRightClickTracking = false;
        ViewportContextMenuState.TrackingViewportIndex = -1;
        ViewportContextMenuState.RightClickTravelSq = 0.0f;
        return;
    }

    InputSystem& IS = InputSystem::Get();
    const FGuiInputState& GuiState = IS.GetGuiInputState();
    if (GuiState.bBlockViewportMouse && !GuiState.bAllowViewportMouseFocus)
    {
        ViewportContextMenuState.bRightClickTracking = false;
        ViewportContextMenuState.TrackingViewportIndex = -1;
        ViewportContextMenuState.RightClickTravelSq = 0.0f;
        return;
    }

    POINT MouseScreenPos = IS.GetMousePos();
    POINT MouseClientPos = Window->ScreenToClientPoint(MouseScreenPos);
    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();

    auto FindViewportAtClientPoint = [&Layout](const POINT& ClientPoint) -> int32
    {
        for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
        {
            const FViewportRect& Rect = Layout.GetSceneViewport(i).GetRect();
            if (Rect.Width <= 0 || Rect.Height <= 0)
            {
                continue;
            }
            if (Rect.Contains(static_cast<int32>(ClientPoint.x), static_cast<int32>(ClientPoint.y)))
            {
                FEditorViewportClient* Client = Layout.GetViewportClient(i);
                const float LocalY = static_cast<float>(ClientPoint.y - Rect.Y);
                if (Client && Client->IsPointerInViewportInputDeadZone(LocalY))
                {
                    return -1;
                }
                return i;
            }
        }
        return -1;
    };

    constexpr float RightClickPopupThresholdPx = 20.0f;
    constexpr float RightClickPopupThresholdSq = RightClickPopupThresholdPx * RightClickPopupThresholdPx;

    if (IS.GetKeyDown(VK_RBUTTON))
    {
        const int32 ViewportIndex = FindViewportAtClientPoint(MouseClientPos);
        if (ViewportIndex >= 0)
        {
            if (FEditorViewportClient* Client = Layout.GetViewportClient(ViewportIndex))
            {
                if (Client->IsPIEPossessed())
                {
                    ViewportContextMenuState.bRightClickTracking = false;
                    ViewportContextMenuState.TrackingViewportIndex = -1;
                    ViewportContextMenuState.RightClickTravelSq = 0.0f;
                    ViewportContextMenuState.PendingPopupViewportIndex = -1;
                    return;
                }
            }

            ViewportContextMenuState.bRightClickTracking = true;
            ViewportContextMenuState.TrackingViewportIndex = ViewportIndex;
            ViewportContextMenuState.RightClickTravelSq = 0.0f;
            ViewportContextMenuState.PressScreenPos = MouseScreenPos;
        }
    }

    if (ViewportContextMenuState.bRightClickTracking && IS.GetKey(VK_RBUTTON))
    {
        const float DeltaX = static_cast<float>(IS.MouseDeltaX());
        const float DeltaY = static_cast<float>(IS.MouseDeltaY());
        ViewportContextMenuState.RightClickTravelSq += DeltaX * DeltaX + DeltaY * DeltaY;
    }

    if (!ViewportContextMenuState.bRightClickTracking || !IS.GetKeyUp(VK_RBUTTON))
    {
        return;
    }

    const int32 ReleaseViewportIndex = FindViewportAtClientPoint(MouseClientPos);
    FEditorViewportClient* TrackingClient =
        Layout.GetViewportClient(ViewportContextMenuState.TrackingViewportIndex);
    if (TrackingClient && TrackingClient->IsPIEPossessed())
    {
        ViewportContextMenuState.bRightClickTracking = false;
        ViewportContextMenuState.TrackingViewportIndex = -1;
        ViewportContextMenuState.RightClickTravelSq = 0.0f;
        ViewportContextMenuState.PendingPopupViewportIndex = -1;
        return;
    }

    const bool bClickCandidate =
        ReleaseViewportIndex == ViewportContextMenuState.TrackingViewportIndex &&
        ViewportContextMenuState.RightClickTravelSq <= RightClickPopupThresholdSq &&
        !IS.GetRightDragging() &&
        !IS.GetRightDragEnd();
    const bool bHasModifier = IS.GetKey(VK_CONTROL) || IS.GetKey(VK_MENU) || IS.GetKey(VK_SHIFT);

    if (bClickCandidate && !bHasModifier)
    {
        Layout.SetLastFocusedViewportIndex(ViewportContextMenuState.TrackingViewportIndex);
        if (FEditorViewportClient* Client =
            Layout.GetViewportClient(ViewportContextMenuState.TrackingViewportIndex))
        {
            POINT PressClientPos = Window->ScreenToClientPoint(ViewportContextMenuState.PressScreenPos);
            const FViewportRect& Rect =
                Layout.GetSceneViewport(ViewportContextMenuState.TrackingViewportIndex).GetRect();
            const float LocalX = MathUtil::Clamp(
                static_cast<float>(PressClientPos.x - Rect.X),
                0.0f,
                static_cast<float>(std::max(0, Rect.Width - 1))
            );
            const float LocalY = MathUtil::Clamp(
                static_cast<float>(PressClientPos.y - Rect.Y),
                0.0f,
                static_cast<float>(std::max(0, Rect.Height - 1))
            );
            Client->RequestSelectAtViewportLocalPoint(LocalX, LocalY, false, false);
            ViewportContextMenuState.PendingSpawnViewportIndex =
                ViewportContextMenuState.TrackingViewportIndex;
            ViewportContextMenuState.PendingSpawnLocalX = LocalX;
            ViewportContextMenuState.PendingSpawnLocalY = LocalY;
        }
        ViewportContextMenuState.PendingPopupViewportIndex =
            ViewportContextMenuState.TrackingViewportIndex;
        ViewportContextMenuState.PendingPopupScreenPos = ImVec2(
            static_cast<float>(ViewportContextMenuState.PressScreenPos.x),
            static_cast<float>(ViewportContextMenuState.PressScreenPos.y + 2)
        );
    }

    ViewportContextMenuState.bRightClickTracking = false;
    ViewportContextMenuState.TrackingViewportIndex = -1;
    ViewportContextMenuState.RightClickTravelSq = 0.0f;
}

void FEditorMainPanel::RenderViewportContextMenu()
{
    if (!EditorEngine)
    {
        return;
    }

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    if (ViewportContextMenuState.PendingPopupViewportIndex >= 0)
    {
        FEditorViewportClient* PendingClient =
            Layout.GetViewportClient(ViewportContextMenuState.PendingPopupViewportIndex);
        if (PendingClient && PendingClient->IsPIEPossessed())
        {
            ViewportContextMenuState.PendingPopupViewportIndex = -1;
            ViewportContextMenuState.PendingSpawnViewportIndex = -1;
            return;
        }

        ImGui::SetNextWindowPos(ViewportContextMenuState.PendingPopupScreenPos, ImGuiCond_Always);
        ImGui::OpenPopup("##ViewportContextMenu");
        ViewportContextMenuState.PendingPopupViewportIndex = -1;
    }

    ImGui::SetNextWindowSizeConstraints(ImVec2(180.0f, 0.0f), ImVec2(260.0f, FLT_MAX));
    if (!ImGui::BeginPopup("##ViewportContextMenu"))
    {
        return;
    }

    const int32 FocusedIndex = Layout.GetLastFocusedViewportIndex();
    FEditorViewportClient* Client = Layout.GetViewportClient(FocusedIndex);
    FEditorViewportState& State = Layout.GetViewportState(FocusedIndex);
    const bool bEditorControl = Client && Client->AllowsEditorWorldControl();
    const bool bPIEActive = Client && Client->IsPIEActive();

	const FWorldContext* Ctx = Client ? EditorEngine->GetWorldContextFromWorld(Client->GetFocusedWorld()) : nullptr;
    const bool bHasSelection = Ctx ? !Ctx->SelectionManager->IsEmpty() : false;

    ImGui::TextDisabled("%s", FEditorMainPanelPlacementHelpers::GetViewportSlotName(FocusedIndex));
    ImGui::Separator();

    if (ImGui::BeginMenu("Place Actor", bEditorControl && Client != nullptr))
    {
        const int32 SpawnViewportIndex =
            ViewportContextMenuState.PendingSpawnViewportIndex >= 0
                ? ViewportContextMenuState.PendingSpawnViewportIndex
                : FocusedIndex;
        FEditorViewportClient* SpawnClient = Layout.GetViewportClient(SpawnViewportIndex);
        const FVector SpawnLocation = FEditorMainPanelPlacementHelpers::ComputePlacementLocation(
            SpawnClient,
            ViewportContextMenuState.PendingSpawnLocalX,
            ViewportContextMenuState.PendingSpawnLocalY
        );

        Widgets.ControlWidget.DrawPlaceActorMenu(SpawnLocation, true);
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Delete", "Del", false, bEditorControl && Client != nullptr && bHasSelection))
    {
        Client->RequestDeleteSelection();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Focus Selection", "F", false, Client != nullptr && bHasSelection))
    {
        Client->FocusSelection();
    }

    if (!bEditorControl)
    {
        if (bPIEActive && ImGui::MenuItem("Stop PIE", "Esc"))
        {
            EditorEngine->StopPlaySession();
        }
        ImGui::EndPopup();
        return;
    }

    if (ImGui::BeginMenu("Transform Mode"))
    {
        if (ImGui::MenuItem("Select", "Q / 1")) Client->RequestSetSelectMode();
        if (ImGui::MenuItem("Translate", "W / 2")) Client->RequestSetTranslateMode();
        if (ImGui::MenuItem("Rotate", "E / 3")) Client->RequestSetRotateMode();
        if (ImGui::MenuItem("Scale", "R / 4")) Client->RequestSetScaleMode();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Selection"))
    {
        if (ImGui::MenuItem("Select All", "Ctrl+A")) Client->RequestSelectAllActors();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, bHasSelection))
        {
            Client->RequestDuplicateSelection();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Layout"))
    {
        struct FLayoutMenuItem
        {
            const char* Label;
            EEditorViewportLayoutMode Mode;
        };
        static constexpr FLayoutMenuItem LayoutItems[] =
        {
            { "One Pane", EEditorViewportLayoutMode::OnePane },
            { "Two Panes Horiz", EEditorViewportLayoutMode::TwoPanesHoriz },
            { "Two Panes Vert", EEditorViewportLayoutMode::TwoPanesVert },
            { "Three Panes Left", EEditorViewportLayoutMode::ThreePanesLeft },
            { "Three Panes Right", EEditorViewportLayoutMode::ThreePanesRight },
            { "Three Panes Top", EEditorViewportLayoutMode::ThreePanesTop },
            { "Three Panes Bottom", EEditorViewportLayoutMode::ThreePanesBottom },
            { "Four Panes 2x2", EEditorViewportLayoutMode::FourPanes2x2 },
            { "Four Panes Left", EEditorViewportLayoutMode::FourPanesLeft },
            { "Four Panes Right", EEditorViewportLayoutMode::FourPanesRight },
            { "Four Panes Top", EEditorViewportLayoutMode::FourPanesTop },
            { "Four Panes Bottom", EEditorViewportLayoutMode::FourPanesBottom },
        };

        const EEditorViewportLayoutMode CurrentMode = Layout.GetLayoutMode();
        for (const FLayoutMenuItem& Item : LayoutItems)
        {
            if (ImGui::MenuItem(Item.Label, nullptr, CurrentMode == Item.Mode))
            {
                const int32 SingleViewportIndex =
                    Item.Mode == EEditorViewportLayoutMode::OnePane ? FocusedIndex : -1;
                Layout.SetLayoutModeAnimated(Item.Mode, SingleViewportIndex);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Toggle Split"))
        {
            Layout.ToggleViewportSplit();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View Mode"))
    {
        static constexpr EViewMode Modes[] =
        {
            EViewMode::Lit_Gouraud,
            EViewMode::Lit_Lambert,
            EViewMode::Lit_BlinnPhong,
            EViewMode::Unlit,
            EViewMode::Heatmap,
            EViewMode::Wireframe,
            EViewMode::Depth,
            EViewMode::Normal,
            EViewMode::IdBuffer,
        };
        static constexpr const char* Labels[] =
        {
            "Lit (Gouraud)",
            "Lit (Lambert)",
            "Lit (Blinn-Phong)",
            "Unlit",
            "Heatmap",
            "Wireframe",
            "Depth",
            "Normal",
            "ID Buffer",
        };
        for (int32 i = 0; i < static_cast<int32>(EViewMode::Count); ++i)
        {
            if (ImGui::MenuItem(Labels[i], nullptr, State.ViewMode == Modes[i]))
            {
                State.ViewMode = Modes[i];
            }
        }
        ImGui::EndMenu();
    }

    ImGui::EndPopup();
}
