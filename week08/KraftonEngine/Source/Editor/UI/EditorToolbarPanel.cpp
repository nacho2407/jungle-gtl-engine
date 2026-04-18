#include "Editor/UI/EditorToolbarPanel.h"

#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/PIE/PIETypes.h"
#include "Editor/Viewport/FLevelViewportLayout.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "ImGui/imgui.h"
#include "Math/MathUtils.h"
#include "Platform/Paths.h"
#include "WICTextureLoader.h"

#include <cstdio>
#include <d3d11.h>

namespace {
template <typename T> void BeginDisabledUnless(bool bEnabled, T &&Fn) {
  if (!bEnabled) {
    ImGui::BeginDisabled();
  }

  Fn();

  if (!bEnabled) {
    ImGui::EndDisabled();
  }
}
} // namespace

void FEditorToolbarPanel::Initialize(UEditorEngine *InEditor,
                                      ID3D11Device *InDevice) {
  Editor = InEditor;
  if (!InDevice) {
    return;
  }

  const std::wstring IconDir =
      FPaths::Combine(FPaths::RootDir(), L"Asset/Editor/Icons/");
  DirectX::CreateWICTextureFromFile(
      InDevice, (IconDir + L"icon_pause_40x.png").c_str(), nullptr, &PlayIcon);
  DirectX::CreateWICTextureFromFile(InDevice,
                                    (IconDir + L"generic_stop_16x.png").c_str(),
                                    nullptr, &StopIcon);
}

void FEditorToolbarPanel::Release() {
  if (PlayIcon) {
    PlayIcon->Release();
    PlayIcon = nullptr;
  }

  if (StopIcon) {
    StopIcon->Release();
    StopIcon = nullptr;
  }

  Editor = nullptr;
}

bool FEditorToolbarPanel::DrawIconButton(const char *Id,
                                          ID3D11ShaderResourceView *Icon,
                                          const char *FallbackLabel,
                                          uint32 TintColor) const {
  bool bClicked = false;

  if (Icon) {
    ImGui::PushID(Id);
    ImGui::InvisibleButton("##IconButton", ImVec2(IconSize, IconSize));

    const ImVec2 Min = ImGui::GetItemRectMin();
    const ImVec2 Max = ImGui::GetItemRectMax();
    ImDrawList *DrawList = ImGui::GetWindowDrawList();

    if (ImGui::IsItemHovered()) {
      DrawList->AddRectFilled(Min, Max, IM_COL32(255, 255, 255, 24), 4.0f);
    }

    DrawList->AddImage(reinterpret_cast<ImTextureID>(Icon), Min, Max,
                       ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), TintColor);

    bClicked = ImGui::IsItemClicked();
    ImGui::PopID();
  } else {
    bClicked =
        ImGui::Button(FallbackLabel, ImVec2(IconSize + 8.0f, IconSize + 8.0f));
  }

  return bClicked;
}

void FEditorToolbarPanel::PushPopupStyle() const {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(PopupPadding, PopupPadding));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
}

void FEditorToolbarPanel::PopPopupStyle() const { ImGui::PopStyleVar(2); }

void FEditorToolbarPanel::RenderPaneToolbar(FLevelViewportLayout *Layout,
                                             int32 SlotIndex,
                                             FLevelEditorViewportClient *VC) {
  if (!Editor || !Layout || !VC) {
    return;
  }

  const FRect &PaneRect = Layout->GetViewportPaneRect(SlotIndex);
  if (PaneRect.Width <= 0 || PaneRect.Height <= 0) {
    return;
  }

  char OverlayID[64];
  std::snprintf(OverlayID, sizeof(OverlayID), "##PaneToolbar_%d", SlotIndex);

  ImGui::SetNextWindowPos(ImVec2(PaneRect.X, PaneRect.Y));
  ImGui::SetNextWindowBgAlpha(1.0f);
  ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f));

  const ImGuiWindowFlags OverlayFlags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

  if (!ImGui::Begin(OverlayID, nullptr, OverlayFlags)) {
    ImGui::End();
    return;
  }

  ImGui::PushID(SlotIndex);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));

  const ImVec2 CursorStart = ImGui::GetCursorScreenPos();
  const float Width = PaneRect.Width;

  ImGui::GetWindowDrawList()->AddRectFilled(
      CursorStart, ImVec2(CursorStart.x + Width, CursorStart.y + ToolbarHeight),
      IM_COL32(40, 40, 40, 255));

  const float ButtonPadding = (ToolbarHeight - IconSize) * 0.5f;
  ImGui::SetCursorScreenPos(
      ImVec2(CursorStart.x + ButtonPadding, CursorStart.y + ButtonPadding));

  const bool bPlaying = Editor->IsPlayingInEditor();

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.15f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.30f));

  const uint32 PlayTint =
      bPlaying ? IM_COL32(255, 255, 255, 255) : IM_COL32(70, 210, 90, 255);
  const uint32 StopTint =
      bPlaying ? IM_COL32(220, 70, 70, 255) : IM_COL32(255, 255, 255, 255);

  if (DrawIconButton("PIE_Play", PlayIcon, "Play", PlayTint)) {
    Layout->SetActiveViewport(VC);

    if (!bPlaying) {
      FRequestPlaySessionParams Params;
      Params.PlayMode = EPIEPlayMode::PlayInViewport;
      Editor->RequestPlaySession(Params);
    }
  }

  ImGui::SameLine(0.0f, ButtonSpacing);

  if (DrawIconButton("PIE_Stop", StopIcon, "Stop", StopTint) && bPlaying) {
    Editor->StopPlayInEditorImmediate();
  }

  ImGui::PopStyleColor(3);

  auto OpenButtonPopup = [&](const char *ButtonLabel, const char *PopupId,
                             auto &&Body) {
    ImGui::SameLine(0.0f, ButtonSpacing);

    if (ImGui::Button(ButtonLabel)) {
      ImGui::OpenPopup(PopupId);
    }

    PushPopupStyle();
    const bool bOpened = ImGui::BeginPopup(PopupId);
    PopPopupStyle();

    if (bOpened) {
      Body();
      ImGui::EndPopup();
    }
  };

  OpenButtonPopup("Layout", "LayoutPopup", [&]() {
    constexpr int32 LayoutCount = static_cast<int32>(EViewportLayout::MAX);
    constexpr int32 Columns = 4;
    constexpr float LayoutIconSize = 32.0f;

    for (int32 i = 0; i < LayoutCount; ++i) {
      ImGui::PushID(i);

      const bool bSelected =
          (static_cast<EViewportLayout>(i) == Layout->GetLayout());
      if (bSelected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
      }

      bool bClicked = false;
      if (ID3D11ShaderResourceView *Icon =
              Layout->GetLayoutIcon(static_cast<EViewportLayout>(i))) {
        bClicked = ImGui::ImageButton("##LayoutIcon",
                                      reinterpret_cast<ImTextureID>(Icon),
                                      ImVec2(LayoutIconSize, LayoutIconSize));
      } else {
        char Label[8];
        std::snprintf(Label, sizeof(Label), "%d", i);
        bClicked = ImGui::Button(
            Label, ImVec2(LayoutIconSize + 8.0f, LayoutIconSize + 8.0f));
      }

      if (bSelected) {
        ImGui::PopStyleColor();
      }

      if (bClicked) {
        Layout->SetLayout(static_cast<EViewportLayout>(i));
        ImGui::CloseCurrentPopup();
      }

      if (((i + 1) % Columns) != 0 && (i + 1) < LayoutCount) {
        ImGui::SameLine();
      }

      ImGui::PopID();
    }
  });

  FViewportRenderOptions &Opts = VC->GetRenderOptions();

  OpenButtonPopup("ViewOrientation", "ViewportTypePopup", [&]() {
    ImGui::SeparatorText("Perspective");
    if (ImGui::Selectable("Perspective", Opts.ViewportType ==
                                             ELevelViewportType::Perspective)) {
      VC->SetViewportType(ELevelViewportType::Perspective);
    }

    ImGui::SeparatorText("Orthographic");

    const struct {
      const char *Label;
      ELevelViewportType Type;
    } OrthoTypes[] = {
        {"Free", ELevelViewportType::FreeOrthographic},
        {"Top", ELevelViewportType::Top},
        {"Bottom", ELevelViewportType::Bottom},
        {"Left", ELevelViewportType::Left},
        {"Right", ELevelViewportType::Right},
        {"Front", ELevelViewportType::Front},
        {"Back", ELevelViewportType::Back},
    };

    for (const auto &Entry : OrthoTypes) {
      if (ImGui::Selectable(Entry.Label, Opts.ViewportType == Entry.Type)) {
        VC->SetViewportType(Entry.Type);
      }
    }

    if (UCameraComponent *Camera = VC->GetCamera()) {
      ImGui::Separator();

      float OrthoWidth = Camera->GetOrthoWidth();
      if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.1f, 0.1f, 1000.0f)) {
        Camera->SetOrthoWidth(Clamp(OrthoWidth, 0.1f, 1000.0f));
      }
    }
  });

  if (UGizmoComponent *Gizmo = Editor->GetGizmo()) {
    OpenButtonPopup("Transform", "GizmoModePopup", [&]() {
      int32 CurrentGizmoMode = static_cast<int32>(Gizmo->GetMode());

      if (ImGui::RadioButton("Translate", &CurrentGizmoMode,
                             static_cast<int32>(EGizmoMode::Translate))) {
        Gizmo->SetTranslateMode();
      }

      if (ImGui::RadioButton("Rotate", &CurrentGizmoMode,
                             static_cast<int32>(EGizmoMode::Rotate))) {
        Gizmo->SetRotateMode();
      }

      if (ImGui::RadioButton("Scale", &CurrentGizmoMode,
                             static_cast<int32>(EGizmoMode::Scale))) {
        Gizmo->SetScaleMode();
      }
    });
  }

  OpenButtonPopup("ViewMode", "ViewModePopup", [&]() {
    int32 CurrentMode = static_cast<int32>(Opts.ViewMode);

    ImGui::RadioButton("Lit_Gouraud", &CurrentMode,
                       static_cast<int32>(EViewMode::Lit_Gouraud));
    ImGui::RadioButton("Lit_Lambert", &CurrentMode,
                       static_cast<int32>(EViewMode::Lit_Lambert));
    ImGui::RadioButton("Lit_Phong", &CurrentMode,
                       static_cast<int32>(EViewMode::Lit_Phong));
    ImGui::RadioButton("Unlit", &CurrentMode,
                       static_cast<int32>(EViewMode::Unlit));
    ImGui::RadioButton("Wireframe", &CurrentMode,
                       static_cast<int32>(EViewMode::Wireframe));
    ImGui::RadioButton("SceneDepth", &CurrentMode,
                       static_cast<int32>(EViewMode::SceneDepth));

    Opts.ViewMode = static_cast<EViewMode>(CurrentMode);

    if (Opts.ViewMode == EViewMode::SceneDepth) {
      ImGui::Separator();
      ImGui::SliderFloat("Exponent", &Opts.Exponent, 1.0f, 512.0f, "%.0f");
      ImGui::Combo("Mode", &Opts.SceneDepthVisMode, "Power\0Linear\0");
    }
  });

  OpenButtonPopup("Show", "ShowPopup", [&]() {
    if (ImGui::CollapsingHeader("Common Show Flags",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Primitives", &Opts.ShowFlags.bPrimitives);
      ImGui::Checkbox("Fog", &Opts.ShowFlags.bFog);
    }

    if (ImGui::CollapsingHeader("Actor Helpers",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Billboard Text", &Opts.ShowFlags.bBillboardText);
      ImGui::Checkbox("Grid", &Opts.ShowFlags.bGrid);

      BeginDisabledUnless(Opts.ShowFlags.bGrid, [&]() {
        ImGui::Indent();
        ImGui::SliderFloat("Spacing", &Opts.GridSpacing, 0.1f, 10.0f, "%.1f");
        ImGui::SliderInt("Half Line Count", &Opts.GridHalfLineCount, 10, 500);
        ImGui::Unindent();
      });

      ImGui::Checkbox("World Axis", &Opts.ShowFlags.bWorldAxis);
      ImGui::Checkbox("Gizmo", &Opts.ShowFlags.bGizmo);
    }

    if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Bounding Volume", &Opts.ShowFlags.bBoundingVolume);
      ImGui::Checkbox("Debug Draw", &Opts.ShowFlags.bDebugDraw);
      ImGui::Checkbox("Octree", &Opts.ShowFlags.bOctree);
    }

    if (ImGui::CollapsingHeader("Post-Processing Show Flags",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Anti-Aliasing (FXAA)", &Opts.ShowFlags.bFXAA);

      BeginDisabledUnless(Opts.ShowFlags.bFXAA, [&]() {
        ImGui::Indent();
        ImGui::SliderFloat("Edge Threshold", &Opts.EdgeThreshold, 0.06f, 0.333f,
                           "%.3f");
        ImGui::SliderFloat("Edge Threshold Min", &Opts.EdgeThresholdMin,
                           0.0312f, 0.0833f, "%.4f");
        ImGui::Unindent();
      });
    }
  });

  ImGui::PopStyleVar(2);
  ImGui::PopID();

  ImGui::SetCursorScreenPos(ImVec2(CursorStart.x, CursorStart.y));
  ImGui::Dummy(ImVec2(Width, ToolbarHeight));
  ImGui::End();
}