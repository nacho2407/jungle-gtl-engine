#include "Editor/UI/EditorMainPanel.h"
#include "Editor/UI/EditorMainPanelViewportToolbarHelpers.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/EditorViewportClient.h"

#include "ImGui/imgui.h"

void FEditorMainPanel::RenderViewportMenuBarForIndex(int32 Index)
{
    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    FEditorViewportClient* Client = Layout.GetViewportClient(Index);
    FEditorViewportState& State = Layout.GetViewportState(Index);

    ImGui::TextDisabled(
        "%s | %s | %s",
        FEditorMainPanelViewportToolbarHelpers::GetViewportSlotName(Index),
        FEditorMainPanelViewportToolbarHelpers::GetViewportTypeName(Client->GetViewportType()),
        FEditorMainPanelViewportToolbarHelpers::GetViewModeName(State.ViewMode));
    ImGui::SameLine();

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
            const bool bSelected = (CurrentMode == Item.Mode);
            if (ImGui::MenuItem(Item.Label, nullptr, bSelected))
            {
                Layout.SetLayoutModeAnimated(Item.Mode, Item.Mode == EEditorViewportLayoutMode::OnePane ? Index : -1);
            }
        }

        if (Layout.IsSingleViewportMode())
        {
            ImGui::Separator();
            for (int32 ViewportSlot = 0; ViewportSlot < FEditorViewportLayout::MaxViewports; ++ViewportSlot)
            {
                const bool bSelected = (Layout.GetSingleViewportIndex() == ViewportSlot);
                if (ImGui::MenuItem(FEditorMainPanelViewportToolbarHelpers::GetViewportSlotName(ViewportSlot), nullptr, bSelected))
                {
                    Layout.SetSingleViewportMode(true, ViewportSlot);
                    Layout.SetLastFocusedViewportIndex(ViewportSlot);
                }
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Toggle Split"))
        {
            Layout.ToggleViewportSplit();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Type"))
    {
        if (Index == 0)
        {
            ImGui::TextDisabled("Viewport 0 is fixed to Perspective.");
            ImGui::Separator();
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
                const bool bSelected = (Client->GetViewportType() == Type);
                if (ImGui::MenuItem(FEditorMainPanelViewportToolbarHelpers::GetViewportTypeName(Type), nullptr, bSelected))
                {
                    Client->SetViewportType(Type);
                    Client->ApplyCameraMode();
                }
            }
        }
        ImGui::EndMenu();
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
            const bool bSelected = (State.ViewMode == Mode);
            if (ImGui::MenuItem(FEditorMainPanelViewportToolbarHelpers::GetViewModeName(Mode), nullptr, bSelected))
            {
                State.ViewMode = Mode;
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
                const bool bSelected = (State.LightCullMode == CullMode);
                if (ImGui::MenuItem(FEditorMainPanelViewportToolbarHelpers::GetLightCullModeName(CullMode), nullptr, bSelected))
                {
                    State.LightCullMode = CullMode;
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Stats"))
    {
        ImGui::MenuItem("FPS", nullptr, &State.bShowStatFPS);
        ImGui::MenuItem("Memory", nullptr, &State.bShowStatMemory);
        ImGui::MenuItem("Cascade Vis", nullptr, &State.bShowCascadeVis);
        ImGui::MenuItem("Light", nullptr, &State.bShowLight);
        ImGui::MenuItem("Shadow", nullptr, &State.bShowShadow);
        ImGui::EndMenu();
    }
}
