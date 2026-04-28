// 에디터 영역의 세부 동작을 구현합니다.
#include "Editor/UI/EditorToolbarPanel.h"

#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/PIE/PIETypes.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Viewport/FLevelViewportLayout.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "GameFramework/AActor.h"
#include "ImGui/imgui.h"
#include "Math/MathUtils.h"
#include "Platform/Paths.h"
#include "Render/Resources/Shadows/ShadowFilterSettings.h"
#include "Render/Resources/Shadows/ShadowMapSettings.h"
#include "WICTextureLoader.h"

#include <cstdio>
#include <d3d11.h>
#include <filesystem>

namespace
{
template <typename T>
void BeginDisabledUnless(bool bEnabled, T&& Fn)
{
    if (!bEnabled)
    {
        ImGui::BeginDisabled();
    }

    Fn();

    if (!bEnabled)
    {
        ImGui::EndDisabled();
    }
}

ID3D11ShaderResourceView* GPlayStartIcon = nullptr;
ID3D11ShaderResourceView* GPauseIcon = nullptr;
ID3D11ShaderResourceView* GStopIcon = nullptr;

void SetFixedPopupPosBelowLastItem(float YOffset)
{
    const ImVec2 ItemMin = ImGui::GetItemRectMin();
    ImGui::SetNextWindowPos(ImVec2(ItemMin.x, ItemMin.y + YOffset), ImGuiCond_Appearing);
}

std::wstring ResolveEditorIconPath(const std::wstring& FileName)
{
    const std::filesystem::path RootCandidate = std::filesystem::path(FPaths::RootDir()) / L"Asset/Editor/Icons" / FileName;
    if (std::filesystem::exists(RootCandidate))
    {
        return RootCandidate.wstring();
    }

    WCHAR Buffer[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, Buffer, MAX_PATH);
    const std::filesystem::path ExeDir = std::filesystem::path(Buffer).parent_path();
    const std::filesystem::path ExeCandidate = ExeDir / L"Asset/Editor/Icons" / FileName;
    if (std::filesystem::exists(ExeCandidate))
    {
        return ExeCandidate.wstring();
    }

    return (std::filesystem::current_path() / L"Asset/Editor/Icons" / FileName).wstring();
}
} // namespace

void FEditorToolbarPanel::Initialize(UEditorEngine* InEditor, ID3D11Device* InDevice)
{
    Editor = InEditor;
    if (!InDevice)
    {
        return;
    }

    if (!GPlayStartIcon)
    {
        DirectX::CreateWICTextureFromFile(
            InDevice,
            ResolveEditorIconPath(L"icon_playInSelectedViewport_16x.png").c_str(),
            nullptr,
            &GPlayStartIcon);
    }

    if (!GPauseIcon)
    {
        DirectX::CreateWICTextureFromFile(
            InDevice,
            ResolveEditorIconPath(L"icon_pause_40x.png").c_str(),
            nullptr,
            &GPauseIcon);
    }

    if (!GStopIcon)
    {
        DirectX::CreateWICTextureFromFile(
            InDevice,
            ResolveEditorIconPath(L"generic_stop_16x.png").c_str(),
            nullptr,
            &GStopIcon);
    }

    PlayIcon = GPlayStartIcon;
    StopIcon = GStopIcon;
}

void FEditorToolbarPanel::Release()
{
    if (GPlayStartIcon)
    {
        GPlayStartIcon->Release();
        GPlayStartIcon = nullptr;
    }

    if (GPauseIcon)
    {
        GPauseIcon->Release();
        GPauseIcon = nullptr;
    }

    if (GStopIcon)
    {
        GStopIcon->Release();
        GStopIcon = nullptr;
    }

    PlayIcon = nullptr;
    StopIcon = nullptr;
    Editor = nullptr;
}

bool FEditorToolbarPanel::DrawIconButton(const char* Id,
                                         ID3D11ShaderResourceView* Icon,
                                         const char* FallbackLabel,
                                         uint32 TintColor) const
{
    bool bClicked = false;

    if (Icon)
    {
        ImGui::PushID(Id);
        ImGui::InvisibleButton("##IconButton", ImVec2(IconSize, IconSize));

        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();

        if (ImGui::IsItemHovered())
        {
            ImGui::GetWindowDrawList()->AddRectFilled(Min, Max, IM_COL32(255, 255, 255, 24), 4.0f);
        }

        ImGui::GetWindowDrawList()->AddImage(
            reinterpret_cast<ImTextureID>(Icon),
            Min,
            Max,
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            TintColor);

        bClicked = ImGui::IsItemClicked();
        ImGui::PopID();
    }
    else
    {
        bClicked = ImGui::Button(FallbackLabel, ImVec2(IconSize, IconSize));
    }

    return bClicked;
}

void FEditorToolbarPanel::PushPopupStyle() const
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
}

void FEditorToolbarPanel::PopPopupStyle() const
{
    ImGui::PopStyleVar(3);
}

void FEditorToolbarPanel::RenderPaneToolbar(FLevelViewportLayout* Layout,
                                            int32 SlotIndex,
                                            FLevelEditorViewportClient* VC)
{
    if (!Editor || !Layout || !VC)
    {
        return;
    }

    const FRect& PaneRect = Layout->GetViewportPaneRect(SlotIndex);
    if (PaneRect.Width <= 0 || PaneRect.Height <= 0)
    {
        return;
    }

    char OverlayID[64];
    std::snprintf(OverlayID, sizeof(OverlayID), "##PaneToolbar_%d", SlotIndex);

    ImGui::SetNextWindowPos(ImVec2(PaneRect.X, PaneRect.Y));
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f));

    const ImGuiWindowFlags OverlayFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove;

    if (!ImGui::Begin(OverlayID, nullptr, OverlayFlags))
    {
        ImGui::End();
        return;
    }

    const float ToolbarHeightPx = ToolbarHeight;
    const float IconSizePx = 24.0f;
    const float ButtonSpacingPx = 8.0f;
    const float PlayStopSpacingPx = 18.0f;
    const float PopupOffsetY = ToolbarHeightPx + 2.0f;
    const float TextButtonHeight = 28.0f;
    const float TextButtonYOffset = -1.0f;

    ImGui::PushID(SlotIndex);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));

    const ImVec2 CursorStart = ImGui::GetCursorScreenPos();
    const float Width = PaneRect.Width;

    ImGui::GetWindowDrawList()->AddRectFilled(
        CursorStart,
        ImVec2(CursorStart.x + Width, CursorStart.y + ToolbarHeightPx),
        IM_COL32(40, 40, 40, 255));

    const float PrevIconSize = IconSize;
    const float PrevToolbarHeight = ToolbarHeight;
    const float PrevButtonSpacing = ButtonSpacing;

    const_cast<FEditorToolbarPanel*>(this)->IconSize = IconSizePx;
    const_cast<FEditorToolbarPanel*>(this)->ToolbarHeight = ToolbarHeightPx;
    const_cast<FEditorToolbarPanel*>(this)->ButtonSpacing = ButtonSpacingPx;

    const float ButtonPaddingX = 8.0f;
    const float IconButtonY = CursorStart.y + (ToolbarHeightPx - IconSizePx) * 0.5f;
    const float TextButtonY = CursorStart.y + (ToolbarHeightPx - TextButtonHeight) * 0.5f + TextButtonYOffset;

    ImGui::SetCursorScreenPos(ImVec2(CursorStart.x + ButtonPaddingX, IconButtonY));

    const bool bPlaying = Editor->IsPlayingInEditor();
    const bool bPaused = Editor->IsPausedInEditor();
    ID3D11ShaderResourceView* CurrentPlayIcon = bPlaying ? GPauseIcon : GPlayStartIcon;

    const uint32 PlayTint = !bPlaying
                                ? IM_COL32(70, 210, 90, 255)
                                : (bPaused ? IM_COL32(255, 230, 80, 255) : IM_COL32(255, 255, 255, 255));
    const uint32 StopTint = bPlaying ? IM_COL32(220, 70, 70, 255) : IM_COL32(255, 255, 255, 255);

    if (DrawIconButton("PIE_Play", CurrentPlayIcon, bPlaying ? (bPaused ? "Resume" : "Pause") : "Play", PlayTint))
    {
        Layout->SetActiveViewport(VC);

        if (!bPlaying)
        {
            FRequestPlaySessionParams Params;
            Params.PlayMode = EPIEPlayMode::PlayInViewport;
            Params.DestinationViewportClient = VC;
            Editor->RequestPlaySession(Params);
        }
        else if (VC->GetPlayState() != EEditorViewportPlayState::Stopped)
        {
            Editor->TogglePausePlayInEditor();
        }
    }

    ImGui::SameLine(0.0f, PlayStopSpacingPx);

    if (DrawIconButton("PIE_Stop", GStopIcon, "Stop", StopTint) && bPlaying)
    {
        Editor->StopPlayInEditorImmediate();
    }

    auto OpenToolbarPopup = [&](const char* ButtonLabel, const char* PopupId, auto&& Body)
    {
        ImGui::SameLine(0.0f, ButtonSpacingPx);

        const ImVec2 CurrentPos = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(CurrentPos.x, TextButtonY));

        if (ImGui::Button(ButtonLabel, ImVec2(0.0f, TextButtonHeight)))
        {
            ImGui::OpenPopup(PopupId);
        }

        SetFixedPopupPosBelowLastItem(PopupOffsetY);
        PushPopupStyle();
        const bool bOpened = ImGui::BeginPopup(PopupId);
        PopPopupStyle();

        if (bOpened)
        {
            Body();
            ImGui::EndPopup();
        }
    };

    OpenToolbarPopup("Layout", "LayoutPopup", [&]()
                     {
        constexpr int32 LayoutCount = static_cast<int32>(EViewportLayout::MAX);
        constexpr int32 Columns = 4;
        constexpr float LayoutIconSize = 28.0f;

        for (int32 i = 0; i < LayoutCount; ++i)
        {
            ImGui::PushID(i);

            const bool bSelected = (static_cast<EViewportLayout>(i) == Layout->GetLayout());
            if (bSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.35f, 0.50f, 1.0f));
            }

            bool bClicked = false;
            if (ID3D11ShaderResourceView* Icon = Layout->GetLayoutIcon(static_cast<EViewportLayout>(i)))
            {
                bClicked = ImGui::ImageButton("##LayoutIcon",
                                              reinterpret_cast<ImTextureID>(Icon),
                                              ImVec2(LayoutIconSize, LayoutIconSize));
            }
            else
            {
                char Label[8];
                std::snprintf(Label, sizeof(Label), "%d", i);
                bClicked = ImGui::Button(Label, ImVec2(LayoutIconSize + 4.0f, LayoutIconSize + 4.0f));
            }

            if (bSelected)
            {
                ImGui::PopStyleColor();
            }

            if (bClicked)
            {
                Layout->SetLayout(static_cast<EViewportLayout>(i));
                ImGui::CloseCurrentPopup();
            }

            if (((i + 1) % Columns) != 0 && (i + 1) < LayoutCount)
            {
                ImGui::SameLine();
            }

            ImGui::PopID();
        } });

    FViewportRenderOptions& Opts = VC->GetRenderOptions();

    OpenToolbarPopup("ViewOrientation", "ViewportTypePopup", [&]()
                     {
        ImGui::SeparatorText("Perspective");
        if (ImGui::Selectable("Perspective", Opts.ViewportType == ELevelViewportType::Perspective))
        {
            VC->SetViewportType(ELevelViewportType::Perspective);
        }

        ImGui::SeparatorText("Orthographic");

        const struct
        {
            const char* Label;
            ELevelViewportType Type;
        } OrthoTypes[] = {
            { "Free",   ELevelViewportType::FreeOrthographic },
            { "Top",    ELevelViewportType::Top },
            { "Bottom", ELevelViewportType::Bottom },
            { "Left",   ELevelViewportType::Left },
            { "Right",  ELevelViewportType::Right },
            { "Front",  ELevelViewportType::Front },
            { "Back",   ELevelViewportType::Back },
        };

        for (const auto& Entry : OrthoTypes)
        {
            if (ImGui::Selectable(Entry.Label, Opts.ViewportType == Entry.Type))
            {
                VC->SetViewportType(Entry.Type);
            }
        }

        if (UCameraComponent* Camera = VC->GetCamera())
        {
            ImGui::Separator();

            float OrthoWidth = Camera->GetOrthoWidth();
            if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.1f, 0.1f, 1000.0f))
            {
                Camera->SetOrthoWidth(Clamp(OrthoWidth, 0.1f, 1000.0f));
            }
        } });

    if (UGizmoComponent* Gizmo = Editor->GetGizmo())
    {
        OpenToolbarPopup("Gizmo", "GizmoModePopup", [&]()
                         {
            int32 CurrentGizmoMode = static_cast<int32>(Gizmo->GetMode());

            if (ImGui::RadioButton("Translate", &CurrentGizmoMode, static_cast<int32>(EGizmoMode::Translate)))
            {
                Gizmo->SetTranslateMode();
            }

            if (ImGui::RadioButton("Rotate", &CurrentGizmoMode, static_cast<int32>(EGizmoMode::Rotate)))
            {
                Gizmo->SetRotateMode();
            }

            if (ImGui::RadioButton("Scale", &CurrentGizmoMode, static_cast<int32>(EGizmoMode::Scale)))
            {
                Gizmo->SetScaleMode();
            } });
    }

    OpenToolbarPopup("Pilot", "PilotPopup", [&]()
                     {
        FSelectionManager& Selection = Editor->GetSelectionManager();
        AActor* SelectedActor = Selection.GetPrimarySelection();
        const bool bCanPilotSelectedActor = (SelectedActor != nullptr);

        BeginDisabledUnless(bCanPilotSelectedActor, [&]()
        {
            FString ActorName = SelectedActor ? SelectedActor->GetFName().ToString() : FString();
            if (ActorName.empty() && SelectedActor && SelectedActor->GetClass())
            {
                ActorName = SelectedActor->GetClass()->GetName();
            }

            FString PilotLabel = "Pilot Selected Actor";
            if (!ActorName.empty())
            {
                PilotLabel += " (" + ActorName + ")";
            }

            if (ImGui::Selectable(PilotLabel.c_str(), false))
            {
                VC->PilotSelectedActor(SelectedActor);
                ImGui::CloseCurrentPopup();
            }
        });

        const bool bIsPilotingActor = VC->IsPilotingActor();
        BeginDisabledUnless(bIsPilotingActor, [&]()
        {
            if (ImGui::Selectable("Stop Piloting Actor", false))
            {
                VC->StopPilotingActor();
                ImGui::CloseCurrentPopup();
            }
        }); });

    OpenToolbarPopup("ViewMode", "ViewModePopup", [&]()
                     {
        int32 CurrentMode = static_cast<int32>(Opts.ViewMode);

        ImGui::RadioButton("Lit_Gouraud", &CurrentMode, static_cast<int32>(EViewMode::Lit_Gouraud));
        ImGui::RadioButton("Lit_Lambert", &CurrentMode, static_cast<int32>(EViewMode::Lit_Lambert));
        ImGui::RadioButton("Lit_Phong", &CurrentMode, static_cast<int32>(EViewMode::Lit_Phong));
        ImGui::RadioButton("Unlit", &CurrentMode, static_cast<int32>(EViewMode::Unlit));
        ImGui::RadioButton("WorldNormal", &CurrentMode, static_cast<int32>(EViewMode::WorldNormal));
        ImGui::RadioButton("Wireframe", &CurrentMode, static_cast<int32>(EViewMode::Wireframe));
        ImGui::RadioButton("SceneDepth", &CurrentMode, static_cast<int32>(EViewMode::SceneDepth));

        Opts.ViewMode = static_cast<EViewMode>(CurrentMode);

        if (Opts.ViewMode == EViewMode::SceneDepth)
        {
            ImGui::Separator();
            ImGui::DragFloat("Exponent", &Opts.Exponent, 0.05f, 0.1f, 10.0f, "%.2f");
            ImGui::DragFloat("Range", &Opts.Range, 0.5f, 0.1f, 1000.0f, "%.1f");
            ImGui::Combo("Mode", &Opts.SceneDepthVisMode, "Power\0Linear\0");
        } });

    OpenToolbarPopup("Show", "ShowPopup", [&]()
                     {
        if (ImGui::CollapsingHeader("Common Show Flags", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Primitives", &Opts.ShowFlags.bPrimitives);
        }

        if (ImGui::CollapsingHeader("Actor Helpers", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Billboard Text", &Opts.ShowFlags.bBillboardText);
            ImGui::Checkbox("Grid", &Opts.ShowFlags.bGrid);

            BeginDisabledUnless(Opts.ShowFlags.bGrid, [&]()
            {
                ImGui::Indent();
                ImGui::SliderFloat("Spacing", &Opts.GridSpacing, 0.1f, 10.0f, "%.1f");
                ImGui::SliderInt("Half Line Count", &Opts.GridHalfLineCount, 10, 500);
                ImGui::Unindent();
            });

            ImGui::Checkbox("World Axis", &Opts.ShowFlags.bWorldAxis);
            ImGui::Checkbox("Gizmo", &Opts.ShowFlags.bGizmo);
        }

        if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Scene BVH (Green)", &Opts.ShowFlags.bSceneBVH);
            ImGui::Checkbox("Scene Octree (Cyan)", &Opts.ShowFlags.bSceneOctree);
            ImGui::Checkbox("World Bound (Magenta)", &Opts.ShowFlags.bWorldBound);
            ImGui::Checkbox("Light Debug Lines", &Opts.ShowFlags.bLightDebugLines);
            ImGui::Checkbox("Light Hit Map", &Opts.ShowFlags.bLightHitMap);

        }

        if (ImGui::CollapsingHeader("Post-Processing Show Flags", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Height Distance Fog", &Opts.ShowFlags.bFog);
            ImGui::Checkbox("Anti-Aliasing (FXAA)", &Opts.ShowFlags.bFXAA);

            BeginDisabledUnless(Opts.ShowFlags.bFXAA, [&]()
            {
                ImGui::Indent();
                ImGui::SliderFloat("Edge Threshold", &Opts.EdgeThreshold, 0.06f, 0.333f, "%.3f");
                ImGui::SliderFloat("Edge Threshold Min", &Opts.EdgeThresholdMin, 0.0312f, 0.0833f, "%.4f");
                ImGui::Unindent();
            });
        }
		if (ImGui::CollapsingHeader("Optimization Options", ImGuiTreeNodeFlags_DefaultOpen))
        {
            int mode = Opts.ShowFlags.b25DCulling ? 1 : 0;

            ImGui::RadioButton("Tile Culling", &mode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("25D Culling", &mode, 1);

            Opts.ShowFlags.b25DCulling = (mode == 1);
        } });

    OpenToolbarPopup("Shadow", "ShadowPopup", [&]()
                     {
        static const char* ShadowMapMethodLabels[] = {
            "Standard",
            "PSM (Perspective Shadow Map)",
            "Cascade"
        };
        int32 SelectedShadowMapMethod = static_cast<int32>(GetShadowMapMethod());
        if (SelectedShadowMapMethod < 0 || SelectedShadowMapMethod >= IM_ARRAYSIZE(ShadowMapMethodLabels))
        {
            SelectedShadowMapMethod = 0;
        }

        if (ImGui::Combo("Shadow Map Method", &SelectedShadowMapMethod, ShadowMapMethodLabels, IM_ARRAYSIZE(ShadowMapMethodLabels)))
        {
            SetShadowMapMethod(static_cast<EShadowMapMethod>(SelectedShadowMapMethod));
        }

        static const char* ShadowFilterLabels[] = {
            "PCF (Percentage-Closer Filtering)",
            "VSM (Variance Shadow Map)",
            "ESM (Exponential Shadow Map)"
        };
        int32 SelectedShadowFilter = static_cast<int32>(GetShadowFilterMethod());
        if (SelectedShadowFilter < 0 || SelectedShadowFilter >= IM_ARRAYSIZE(ShadowFilterLabels))
        {
            SelectedShadowFilter = 0;
        }

        if (ImGui::Combo("Shadow Filter", &SelectedShadowFilter, ShadowFilterLabels, IM_ARRAYSIZE(ShadowFilterLabels)))
        {
            SetShadowFilterMethod(static_cast<EShadowFilterMethod>(SelectedShadowFilter));
        }

        ImGui::TextDisabled("Global renderer settings.");
        ImGui::TextDisabled("Applied to all shadow-casting lights.");
    });

    const_cast<FEditorToolbarPanel*>(this)->IconSize = PrevIconSize;
    const_cast<FEditorToolbarPanel*>(this)->ToolbarHeight = PrevToolbarHeight;
    const_cast<FEditorToolbarPanel*>(this)->ButtonSpacing = PrevButtonSpacing;

    ImGui::PopStyleVar(2);
    ImGui::PopID();

    ImGui::SetCursorScreenPos(ImVec2(CursorStart.x, CursorStart.y));
    ImGui::Dummy(ImVec2(Width, ToolbarHeightPx));
    ImGui::End();
}
