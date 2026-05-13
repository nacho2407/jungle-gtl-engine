#include "Editor/UI/EditorMainPanel.h"
#include "Editor/UI/EditorMainPanelViewportToolbarHelpers.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Engine/Component/GizmoComponent.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cstdio>

void FEditorMainPanel::RenderViewportIconToolbarForIndex(int32 ViewportIndex)
{
    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    FEditorViewportClient* Client = Layout.GetViewportClient(ViewportIndex);
    if (!Client)
    {
        return;
    }

    const bool bEditorControl = Client->AllowsEditorWorldControl();
    const bool bPIEPossessed = Client->IsPIEPossessed();

    ImGui::PushID(ViewportIndex);
    if (bPIEPossessed)
    {
        ImGui::PopID();
        return;
    }

    constexpr float ToolbarLeftPadding = 8.0f;
    const float CenteredToolbarY = std::max(0.0f, (ImGui::GetWindowHeight() - ImGui::GetFrameHeight()) * 0.5f);
    ImGui::SetCursorPos(ImVec2(ToolbarLeftPadding, CenteredToolbarY));

    if (DrawViewportIconButton(
        "##SelectMode",
        EEditorMainPanelViewportToolIcon::Select,
        "Q",
        "Select (Q / 1)",
        Client->GetTransformMode() == FEditorViewportClient::ETransformMode::Select,
        bEditorControl))
    {
        Client->RequestSetSelectMode();
    }
    ImGui::SameLine();
    if (DrawViewportIconButton(
        "##TranslateMode",
        EEditorMainPanelViewportToolIcon::Translate,
        "W",
        "Translate (W / 2)",
        Client->GetTransformMode() == FEditorViewportClient::ETransformMode::Translate,
        bEditorControl))
    {
        Client->RequestSetTranslateMode();
    }
    ImGui::SameLine();
    if (DrawViewportIconButton(
        "##RotateMode",
        EEditorMainPanelViewportToolIcon::Rotate,
        "E",
        "Rotate (E / 3)",
        Client->GetTransformMode() == FEditorViewportClient::ETransformMode::Rotate,
        bEditorControl))
    {
        Client->RequestSetRotateMode();
    }
    ImGui::SameLine();
    if (DrawViewportIconButton(
        "##ScaleMode",
        EEditorMainPanelViewportToolIcon::Scale,
        "R",
        "Scale (R / 4)",
        Client->GetTransformMode() == FEditorViewportClient::ETransformMode::Scale,
        bEditorControl))
    {
        Client->RequestSetScaleMode();
    }

    ImGui::SameLine(0.0f, 10.0f);
    {
        const ImVec2 SeparatorStart = ImGui::GetCursorScreenPos();
        const float SeparatorHeight = ImGui::GetFrameHeight() - 4.0f;
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(SeparatorStart.x, SeparatorStart.y + 2.0f),
            ImVec2(SeparatorStart.x, SeparatorStart.y + 2.0f + SeparatorHeight),
            IM_COL32(155, 155, 155, 255),
            1.0f);
        ImGui::Dummy(ImVec2(1.0f, ImGui::GetFrameHeight()));
    }

    ImGui::SameLine(0.0f, 10.0f);
    const bool bWorldSpace = !Client->GetGizmo() || Client->GetGizmo()->IsWorldSpace();
    if (DrawViewportIconButton(
        "##SpaceMode",
        bWorldSpace ? EEditorMainPanelViewportToolIcon::WorldSpace : EEditorMainPanelViewportToolIcon::LocalSpace,
        bWorldSpace ? "W" : "L",
        bWorldSpace ? "World Space (X)" : "Local Space (X)",
        bWorldSpace,
        bEditorControl))
    {
        Client->RequestToggleCoordinateSpace();
    }

    static bool GTranslateSnapEnabled[FEditorViewportLayout::MaxViewports] = {};
    static bool GRotateSnapEnabled[FEditorViewportLayout::MaxViewports] = {};
    static bool GScaleSnapEnabled[FEditorViewportLayout::MaxViewports] = {};
    static int32 GTranslateSnapIndex[FEditorViewportLayout::MaxViewports] = { 1, 1, 1, 1 };
    static int32 GRotateSnapIndex[FEditorViewportLayout::MaxViewports] = { 1, 1, 1, 1 };
    static int32 GScaleSnapIndex[FEditorViewportLayout::MaxViewports] = { 1, 1, 1, 1 };
    static constexpr const char* TranslateSnapLabels[] = { "1", "5", "10", "50", "100" };
    static constexpr const char* RotateSnapLabels[] = { "5", "10", "15", "30", "45" };
    static constexpr const char* ScaleSnapLabels[] = { "0.1", "0.25", "0.5", "1.0", "5.0" };
    static constexpr float TranslateSnapValues[] = { 1.0f, 5.0f, 10.0f, 50.0f, 100.0f };
    static constexpr float RotateSnapValues[] = { 5.0f, 10.0f, 15.0f, 30.0f, 45.0f };
    static constexpr float ScaleSnapValues[] = { 0.1f, 0.25f, 0.5f, 1.0f, 5.0f };

    auto DrawSnapSection = [&](
        EEditorMainPanelViewportToolIcon SnapIcon,
        const char* Prefix,
        bool& bEnabled,
        int32& ValueIndex,
        const char* const* Labels,
        int32 LabelCount)
    {
        ImGui::SameLine(0.0f, 6.0f);
        if (bEnabled)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.43f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.50f, 0.36f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.36f, 0.26f, 1.0f));
        }

        char ToggleID[48];
        snprintf(ToggleID, sizeof(ToggleID), "##%sSnapToggle", Prefix);
        const bool bTogglePressed = DrawViewportIconButton(ToggleID, SnapIcon, Prefix, Prefix, false, bEditorControl, true, false);
        if (bEnabled)
        {
            ImGui::PopStyleColor(3);
        }
        if (bTogglePressed)
        {
            bEnabled = !bEnabled;
        }

        ImGui::SameLine(0.0f, 0.0f);
        char PopupID[48];
        snprintf(PopupID, sizeof(PopupID), "##%sSnapPopup", Prefix);
        char ValueBtnID[48];
        snprintf(ValueBtnID, sizeof(ValueBtnID), "##%sSnapValueButton", Prefix);
        char Label[24];
        snprintf(Label, sizeof(Label), "%s ▼", Labels[ValueIndex]);
        if (DrawViewportTextButton(ValueBtnID, Label, false, true))
        {
            ImGui::OpenPopup(PopupID);
        }
        if (ImGui::BeginPopup(PopupID))
        {
            for (int32 i = 0; i < LabelCount; ++i)
            {
                const bool bSelected = (ValueIndex == i);
                if (ImGui::RadioButton(Labels[i], bSelected))
                {
                    ValueIndex = i;
                }
            }
            ImGui::EndPopup();
        }
    };

    if (Layout.GetLayoutMode() == EEditorViewportLayoutMode::OnePane)
    {
        DrawSnapSection(
            EEditorMainPanelViewportToolIcon::TranslateSnap,
            "T",
            GTranslateSnapEnabled[ViewportIndex],
            GTranslateSnapIndex[ViewportIndex],
            TranslateSnapLabels,
            IM_ARRAYSIZE(TranslateSnapLabels));
        DrawSnapSection(
            EEditorMainPanelViewportToolIcon::RotateSnap,
            "R",
            GRotateSnapEnabled[ViewportIndex],
            GRotateSnapIndex[ViewportIndex],
            RotateSnapLabels,
            IM_ARRAYSIZE(RotateSnapLabels));
        DrawSnapSection(
            EEditorMainPanelViewportToolIcon::ScaleSnap,
            "S",
            GScaleSnapEnabled[ViewportIndex],
            GScaleSnapIndex[ViewportIndex],
            ScaleSnapLabels,
            IM_ARRAYSIZE(ScaleSnapLabels));
    }

    if (Client->GetGizmo() && Layout.GetLastFocusedViewportIndex() == ViewportIndex)
    {
        Client->GetGizmo()->SetTranslateSnap(
            GTranslateSnapEnabled[ViewportIndex],
            TranslateSnapValues[GTranslateSnapIndex[ViewportIndex]]);
        Client->GetGizmo()->SetRotateSnap(
            GRotateSnapEnabled[ViewportIndex],
            RotateSnapValues[GRotateSnapIndex[ViewportIndex]]);
        Client->GetGizmo()->SetScaleSnap(
            GScaleSnapEnabled[ViewportIndex],
            ScaleSnapValues[GScaleSnapIndex[ViewportIndex]]);
    }

    const ImVec2 WindowScreenPos = ImGui::GetWindowPos();
    const float LeftGroupEndX = ImGui::GetItemRectMax().x - WindowScreenPos.x;

    char TypePopupID[48];
    snprintf(TypePopupID, sizeof(TypePopupID), "##ViewportTypePopup_%d", ViewportIndex);
    char TypeButtonLabel[64];
    snprintf(TypeButtonLabel, sizeof(TypeButtonLabel), "%s ▼", FEditorMainPanelViewportToolbarHelpers::GetViewportTypeName(Client->GetViewportType()));
    char CameraPopupID[48];
    snprintf(CameraPopupID, sizeof(CameraPopupID), "##ViewportCameraSpeedPopup_%d", ViewportIndex);
    char CameraButtonLabel[48];
    snprintf(CameraButtonLabel, sizeof(CameraButtonLabel), "Cam %.1fx ▼", FEditorMainPanelViewportToolbarHelpers::GetCameraSpeedMultiplier(Client));
    char ViewPopupID[48];
    snprintf(ViewPopupID, sizeof(ViewPopupID), "##ViewportViewPopup_%d", ViewportIndex);
    char ViewButtonLabel[80];
    snprintf(ViewButtonLabel, sizeof(ViewButtonLabel), "%s ▼", FEditorMainPanelViewportToolbarHelpers::GetViewModeName(Client->GetViewportState()->ViewMode));

    const ImVec2 FramePadding = ImGui::GetStyle().FramePadding;
    auto CalcTextButtonWidth = [&](const char* Label) -> float
    {
        return ImGui::CalcTextSize(Label).x + FramePadding.x * 2.0f;
    };
    constexpr float IconButtonWidth = 16.0f + 5.0f * 2.0f;
    float RightGroupWidth = 0.0f;
    int32 RightItemCount = 0;
    auto AddRightItemWidth = [&](float Width)
    {
        if (RightItemCount > 0)
        {
            RightGroupWidth += ImGui::GetStyle().ItemSpacing.x;
        }
        RightGroupWidth += Width;
        ++RightItemCount;
    };
    AddRightItemWidth(CalcTextButtonWidth(TypeButtonLabel));
    if (Layout.GetLayoutMode() == EEditorViewportLayoutMode::OnePane)
    {
        AddRightItemWidth(CalcTextButtonWidth(CameraButtonLabel));
    }
    AddRightItemWidth(CalcTextButtonWidth(ViewButtonLabel));
    AddRightItemWidth(IconButtonWidth);
    AddRightItemWidth(IconButtonWidth);
    AddRightItemWidth(IconButtonWidth);

    const float ContentRightX = ImGui::GetWindowContentRegionMax().x;
    const float RightStartX = ContentRightX - RightGroupWidth;
    const float MinRightGroupStartX = LeftGroupEndX + ImGui::GetStyle().ItemSpacing.x + 12.0f;
    const bool bUseOverflowMenu = RightStartX <= MinRightGroupStartX;
    auto SetToolbarItemScreenPos = [&](float LocalX)
    {
        ImGui::SetCursorScreenPos(ImVec2(WindowScreenPos.x + std::max(0.0f, LocalX), WindowScreenPos.y + CenteredToolbarY));
    };

    if (bUseOverflowMenu)
    {
        const float OverflowStartX = ContentRightX - IconButtonWidth;
        SetToolbarItemScreenPos(OverflowStartX);
        if (DrawViewportIconButton(
            "##ViewportToolbarOverflow",
            EEditorMainPanelViewportToolIcon::Menu,
            "...",
            "Viewport Toolbar",
            false,
            true))
        {
            ImGui::OpenPopup("##ViewportToolbarOverflowPopup");
        }

        if (ImGui::BeginPopup("##ViewportToolbarOverflowPopup"))
        {
            if (ImGui::BeginMenu("Type"))
            {
                if (ViewportIndex == 0)
                {
                    ImGui::MenuItem("Perspective", nullptr, true, false);
                }
                else
                {
                    static constexpr EEditorViewportType OrthoTypes[] = {
                        EVT_OrthoTop,
                        EVT_OrthoBottom,
                        EVT_OrthoFront,
                        EVT_OrthoBack,
                        EVT_OrthoLeft,
                        EVT_OrthoRight,
                    };
                    for (EEditorViewportType Type : OrthoTypes)
                    {
                        if (ImGui::MenuItem(FEditorMainPanelViewportToolbarHelpers::GetViewportTypeName(Type), nullptr, Client->GetViewportType() == Type))
                        {
                            Client->SetViewportType(Type);
                            Client->ApplyCameraMode();
                        }
                    }
                }
                ImGui::EndMenu();
            }

            if (Layout.GetLayoutMode() == EEditorViewportLayoutMode::OnePane)
            {
                float SpeedMultiplier = FEditorMainPanelViewportToolbarHelpers::GetCameraSpeedMultiplier(Client);
                if (ImGui::SliderFloat("Camera Speed", &SpeedMultiplier, 0.01f, FEditorMainPanelViewportToolbarHelpers::MaxCameraSpeedMultiplier, "%.2fx"))
                {
                    FEditorMainPanelViewportToolbarHelpers::SetCameraSpeedMultiplier(Client, SpeedMultiplier);
                }
                ImGui::Separator();
            }

            if (ImGui::BeginMenu("View"))
            {
                static constexpr EViewMode Modes[] = {
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
                for (EViewMode Mode : Modes)
                {
                    if (ImGui::MenuItem(FEditorMainPanelViewportToolbarHelpers::GetViewModeName(Mode), nullptr, Client->GetViewportState()->ViewMode == Mode))
                    {
                        Client->GetViewportState()->ViewMode = Mode;
                    }
                }

                ImGui::Separator();
                if (ImGui::BeginMenu("Light Culling"))
                {
                    static constexpr ELightCullMode CullModes[] = {
                        ELightCullMode::Clustered,
                        ELightCullMode::Tiled,
                        ELightCullMode::None,
                    };
                    for (ELightCullMode CullMode : CullModes)
                    {
                        if (ImGui::MenuItem(
                            FEditorMainPanelViewportToolbarHelpers::GetLightCullModeName(CullMode),
                            nullptr,
                            Client->GetViewportState()->LightCullMode == CullMode))
                        {
                            Client->GetViewportState()->LightCullMode = CullMode;
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            const bool bSettingsVisible = Widgets.ViewportOverlayWidget.IsViewportSettingsVisible();
            if (ImGui::MenuItem("Viewport Settings", nullptr, bSettingsVisible))
            {
                Widgets.ViewportOverlayWidget.SetViewportSettingsVisible(!bSettingsVisible);
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
                        Layout.SetLayoutModeAnimated(
                            Item.Mode,
                            Item.Mode == EEditorViewportLayoutMode::OnePane ? ViewportIndex : -1);
                    }
                }
                ImGui::EndMenu();
            }

            const EEditorViewportLayoutMode CurrentLayout = Layout.GetLayoutMode();
            if (ImGui::MenuItem(CurrentLayout == EEditorViewportLayoutMode::OnePane ? "Split Viewport" : "Merge Viewport"))
            {
                Layout.SetLastFocusedViewportIndex(ViewportIndex);
                Layout.ToggleViewportSplit();
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();
        return;
    }

    SetToolbarItemScreenPos(RightStartX);

    if (DrawViewportTextButton("##ViewportTypeButton", TypeButtonLabel))
    {
        ImGui::OpenPopup(TypePopupID);
    }
    if (ImGui::BeginPopup(TypePopupID))
    {
        if (ViewportIndex == 0)
        {
            ImGui::MenuItem("Perspective", nullptr, true, false);
        }
        else
        {
            static constexpr EEditorViewportType OrthoTypes[] = {
                EVT_OrthoTop,
                EVT_OrthoBottom,
                EVT_OrthoFront,
                EVT_OrthoBack,
                EVT_OrthoLeft,
                EVT_OrthoRight,
            };
            for (EEditorViewportType Type : OrthoTypes)
            {
                if (ImGui::MenuItem(FEditorMainPanelViewportToolbarHelpers::GetViewportTypeName(Type), nullptr, Client->GetViewportType() == Type))
                {
                    Client->SetViewportType(Type);
                    Client->ApplyCameraMode();
                }
            }
        }
        ImGui::EndPopup();
    }

    if (Layout.GetLayoutMode() == EEditorViewportLayoutMode::OnePane)
    {
        ImGui::SameLine();
        if (DrawViewportTextButton("##ViewportCameraSpeedButton", CameraButtonLabel))
        {
            ImGui::OpenPopup(CameraPopupID);
        }
        if (ImGui::BeginPopup(CameraPopupID))
        {
            float SpeedMultiplier = FEditorMainPanelViewportToolbarHelpers::GetCameraSpeedMultiplier(Client);
            if (ImGui::SliderFloat("Speed", &SpeedMultiplier, 0.01f, FEditorMainPanelViewportToolbarHelpers::MaxCameraSpeedMultiplier, "%.2fx"))
            {
                FEditorMainPanelViewportToolbarHelpers::SetCameraSpeedMultiplier(Client, SpeedMultiplier);
            }
            ImGui::EndPopup();
        }
    }

    ImGui::SameLine();
    if (DrawViewportTextButton("##ViewportViewButton", ViewButtonLabel))
    {
        ImGui::OpenPopup(ViewPopupID);
    }
    if (ImGui::BeginPopup(ViewPopupID))
    {
        static constexpr EViewMode Modes[] = {
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
        for (EViewMode Mode : Modes)
        {
            if (ImGui::MenuItem(FEditorMainPanelViewportToolbarHelpers::GetViewModeName(Mode), nullptr, Client->GetViewportState()->ViewMode == Mode))
            {
                Client->GetViewportState()->ViewMode = Mode;
            }
        }

        ImGui::Separator();
        if (ImGui::BeginMenu("Light Culling"))
        {
            static constexpr ELightCullMode CullModes[] = {
                ELightCullMode::Clustered,
                ELightCullMode::Tiled,
                ELightCullMode::None,
            };
            for (ELightCullMode CullMode : CullModes)
            {
                if (ImGui::MenuItem(
                    FEditorMainPanelViewportToolbarHelpers::GetLightCullModeName(CullMode),
                    nullptr,
                    Client->GetViewportState()->LightCullMode == CullMode))
                {
                    Client->GetViewportState()->LightCullMode = CullMode;
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (DrawViewportIconButton(
        "##ViewportSettings",
        EEditorMainPanelViewportToolIcon::Setting,
        "S",
        "Viewport Settings",
        Widgets.ViewportOverlayWidget.IsViewportSettingsVisible(),
        true))
    {
        Widgets.ViewportOverlayWidget.SetViewportSettingsVisible(!Widgets.ViewportOverlayWidget.IsViewportSettingsVisible());
    }

    ImGui::SameLine();
    if (DrawViewportIconButton(
        "##LayoutIconMenu",
        EEditorMainPanelViewportToolIcon::Menu,
        "L",
        "Layout Presets",
        false,
        true,
        true,
        false))
    {
        ImGui::OpenPopup("##LayoutIconPopup");
    }

    if (ImGui::BeginPopup("##LayoutIconPopup"))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
        constexpr int32 Columns = 4;
        constexpr ImVec2 IconSize(32.0f, 32.0f);
        const EEditorViewportLayoutMode CurrentMode = Layout.GetLayoutMode();
        for (int32 i = 0; i < static_cast<int32>(EEditorViewportLayoutMode::Max); ++i)
        {
            ImGui::PushID(i);
            const EEditorViewportLayoutMode Mode = static_cast<EEditorViewportLayoutMode>(i);
            const bool bSelected = (Mode == CurrentMode);
            if (bSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.33f, 0.46f, 0.63f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.37f, 0.52f, 0.70f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.27f, 0.39f, 0.54f, 1.0f));
            }

            bool bPressed = false;
            if (IconResources.LayoutIcons[i])
            {
                bPressed = ImGui::ImageButton("##LayoutIcon", reinterpret_cast<void*>(IconResources.LayoutIcons[i]), IconSize);
            }
            else
            {
                bPressed = ImGui::Button(FEditorMainPanelViewportToolbarHelpers::GetViewportLayoutLabel(Mode), ImVec2(110.0f, 0.0f));
            }

            if (bSelected)
            {
                ImGui::PopStyleColor(3);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", FEditorMainPanelViewportToolbarHelpers::GetViewportLayoutLabel(Mode));
            }
            if (bPressed)
            {
                Layout.SetLayoutModeAnimated(Mode, Mode == EEditorViewportLayoutMode::OnePane ? ViewportIndex : -1);
                ImGui::CloseCurrentPopup();
            }

            if ((i + 1) % Columns != 0)
            {
                ImGui::SameLine();
            }
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, 0.0f);
    const EEditorViewportLayoutMode CurrentLayout = Layout.GetLayoutMode();
    const EEditorViewportLayoutMode ToggleLayout =
        CurrentLayout == EEditorViewportLayoutMode::OnePane
            ? EEditorViewportLayoutMode::FourPanes2x2
            : EEditorViewportLayoutMode::OnePane;
    const int32 ToggleLayoutIndex = static_cast<int32>(ToggleLayout);
    ID3D11ShaderResourceView* ToggleIcon =
        (ToggleLayoutIndex >= 0 && ToggleLayoutIndex < static_cast<int32>(EEditorViewportLayoutMode::Max))
            ? IconResources.LayoutIcons[ToggleLayoutIndex]
            : nullptr;
    const char* ToggleTooltip = CurrentLayout == EEditorViewportLayoutMode::OnePane ? "Split Viewport" : "Merge Viewport";
    bool bTogglePressed = false;
    if (ToggleIcon)
    {
        constexpr ImVec2 IconSize(16.0f, 16.0f);
        const ImVec2 Padding = ImGui::GetStyle().FramePadding;
        const ImVec2 ButtonSize(IconSize.x + Padding.x * 2.0f, ImGui::GetFrameHeight());
        bTogglePressed = ImGui::InvisibleButton("##SplitMergeViewport", ButtonSize);
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        const bool bHovered = ImGui::IsItemHovered();
        const bool bHeld = ImGui::IsItemActive();
        const ImU32 BgColor = ImGui::GetColorU32(bHeld ? ImGuiCol_ButtonActive : (bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
        ImGui::GetWindowDrawList()->AddRectFilled(Min, Max, BgColor, ImGui::GetStyle().FrameRounding, ImDrawFlags_RoundCornersRight);
        ImGui::GetWindowDrawList()->AddImage(
            reinterpret_cast<ImTextureID>(ToggleIcon),
            ImVec2(Min.x + Padding.x, Min.y + (ButtonSize.y - IconSize.y) * 0.5f),
            ImVec2(Min.x + Padding.x + IconSize.x, Min.y + (ButtonSize.y + IconSize.y) * 0.5f));
    }
    else
    {
        bTogglePressed = DrawViewportTextButton(
            "##SplitMergeViewportText",
            CurrentLayout == EEditorViewportLayoutMode::OnePane ? "Split" : "Merge",
            false,
            true);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", ToggleTooltip);
    }
    if (bTogglePressed)
    {
        Layout.SetLastFocusedViewportIndex(ViewportIndex);
        Layout.ToggleViewportSplit();
    }
    ImGui::PopID();
}
