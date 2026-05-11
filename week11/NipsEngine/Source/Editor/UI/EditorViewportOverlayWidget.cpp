#include "Editor/UI/EditorViewportOverlayWidget.h"

#include "Core/ResourceManager.h"
#include "Editor/EditorEngine.h"
#include "Editor/EditorRenderPipeline.h"
#include "Editor/Settings/EditorSettings.h"
#include "Engine/Slate/SlateApplication.h"
#include "ImGui/imgui.h"
#include "Engine/Object/ObjectIterator.h"
#include "Engine/Asset/StaticMesh.h"
#include "Engine/Asset/StaticMeshTypes.h"
#include "Engine/Component/GizmoComponent.h"
#include "Engine/Object/FName.h"
#include "Render/Resource/ShadowAtlasManager.h"
#include <cstdio>
#include "Slate/SSplitterV.h"
#include "Slate/SSplitterH.h"
#include "Slate/SSplitterCross.h"
#include "Viewport/ViewportLayout.h"
#include "Input/InputSystem.h"
#include <initializer_list>
#include <utility>
#include <algorithm>

namespace
{
	constexpr float AtlasGridCellPixels = 256.0f;
	constexpr float AtlasZoomRegionUv = 1.0f / 16.0f;

	float ClampFloat(float Value, float MinValue, float MaxValue)
	{
		return std::max(MinValue, std::min(Value, MaxValue));
	}

	float GetCameraBaseSpeed()
	{
		return std::max(0.1f, FEditorSettings::Get().CameraSpeed);
	}

	float GetCameraSpeedMultiplier(FEditorViewportClient* Client)
	{
		return Client ? Client->GetMoveSpeed() / GetCameraBaseSpeed() : 1.0f;
	}

	void SetCameraSpeedMultiplier(FEditorViewportClient* Client, float Multiplier)
	{
		if (!Client)
		{
			return;
		}
		Client->SetMoveSpeed(ClampFloat(GetCameraBaseSpeed() * Multiplier, 0.1f, 5000.0f));
	}

	ImVec2 Add(const ImVec2& A, const ImVec2& B)
	{
		return ImVec2(A.x + B.x, A.y + B.y);
	}

	ImVec2 Subtract(const ImVec2& A, const ImVec2& B)
	{
		return ImVec2(A.x - B.x, A.y - B.y);
	}

	ImU32 ToU32(const ImVec4& Color)
	{
		return ImGui::ColorConvertFloat4ToU32(Color);
	}

	ImVec2 AtlasPixelToPreview(const ImVec2& ImagePos, const ImVec2& ImageSize, int32 X, int32 Y)
	{
		const float AtlasSize = static_cast<float>(ShadowAtlasResolution2D);
		return ImVec2(
			ImagePos.x + ImageSize.x * (static_cast<float>(X) / AtlasSize),
			ImagePos.y + ImageSize.y * (static_cast<float>(Y) / AtlasSize));
	}
} // namespace

// 뷰포트 타입 → 표시 이름
static const char* GetViewportTypeName(EEditorViewportType Type)
{
	switch (Type)
	{
	case EVT_Perspective: return "Perspective";
	case EVT_OrthoTop:    return "Top";
	case EVT_OrthoBottom: return "Bottom";
	case EVT_OrthoFront:  return "Front";
	case EVT_OrthoBack:   return "Back";
	case EVT_OrthoLeft:   return "Left";
	case EVT_OrthoRight:  return "Right";
	default:              return "Viewport";
	}
}

static const char* GetViewModeName(EViewMode Mode)
{
	switch (Mode)
	{
	case EViewMode::Lit_Gouraud:   return "Lit (Gouraud)";
	case EViewMode::Lit_Lambert:   return "Lit (Lambert)";
	case EViewMode::Lit_BlinnPhong: return "Lit (Blinn-Phong)";
	case EViewMode::Unlit:     return "Unlit";
	case EViewMode::Heatmap:   return "Heatmap";
	case EViewMode::Wireframe: return "Wireframe";
	case EViewMode::Depth:     return "Depth";
	case EViewMode::Normal:    return "Normal";
	default:                   return "Lit";
	}
}

static const char* GetLightCullModeName(ELightCullMode Mode)
{
	switch (Mode)
	{
	case ELightCullMode::Clustered: return "Clustered";
	case ELightCullMode::Tiled:     return "Tiled";
	case ELightCullMode::None:      return "None (All Lights)";
	default:                        return "Clustered";
	}
}

static const char* GetShadowFilterName(EShadowFilter Mode)
{
	switch (Mode)
	{
	case EShadowFilter::VSM: return "VSM";
	case EShadowFilter::PCF:
	default:
		return "PCF";
	}
}

void FEditorViewportOverlayWidget::Render(float DeltaTime)
{
	RenderViewportFrameOverlays(DeltaTime);
	RenderFloatingOverlays(DeltaTime);
}

void FEditorViewportOverlayWidget::RenderViewportFrameOverlays(float DeltaTime)
{
	(void)DeltaTime;
	RenderBoxSelectionOverlay();
}

void FEditorViewportOverlayWidget::RenderFloatingOverlays(float DeltaTime)
{
	if (bShowViewportSettings)
	{
		RenderViewportSettings(DeltaTime);
	}
	RenderDebugStats(DeltaTime);
	RenderGroupedStatOverlay(DeltaTime);
    RenderShortcutsWindow();
}

void FEditorViewportOverlayWidget::RenderViewportSettings(float DeltaTime)
{
    (void)DeltaTime;
    FEditorSettings& Settings = FEditorSettings::Get();

    ImVec2 OverlayPos(24.0f, 82.0f);
    ImGuiID OverlayViewportID = 0;
    if (EditorEngine)
    {
        FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
        const int32 FocusedIdx = Layout.GetLastFocusedViewportIndex();
        const FViewportRect& Rect = Layout.GetSceneViewport(FocusedIdx).GetRect();
        if (Rect.Width > 1 && Rect.Height > 1)
        {
            OverlayPos = ImVec2(
                static_cast<float>(Rect.X + Rect.Width) - 340.0f,
                static_cast<float>(Rect.Y) + 42.0f);
        }
    }
    if (const ImGuiViewport* MainViewport = ImGui::GetMainViewport())
    {
        OverlayViewportID = MainViewport->ID;
    }

    ImGui::SetNextWindowPos(OverlayPos, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f, 80.0f), ImVec2(360.0f, 620.0f));
    if (OverlayViewportID != 0)
    {
        ImGui::SetNextWindowViewport(OverlayViewportID);
    }
    ImGui::SetNextWindowBgAlpha(0.92f);

    constexpr ImGuiWindowFlags Flags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.08f, 0.10f, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.28f, 0.34f, 0.46f, 0.85f));

    bool bOpen = bShowViewportSettings;
    if (!ImGui::Begin("Viewport Settings", &bOpen, Flags))
    {
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
        bShowViewportSettings = bOpen;
        return;
    }
    bShowViewportSettings = bOpen;

    float ItemWidth = ImGui::GetContentRegionAvail().x * 0.5f;
    FEditorViewportState* FocusedState = nullptr;
    FEditorViewportClient* FocusedClient = nullptr;
    if (EditorEngine)
    {
        FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
        const int32 FocusedIdx = Layout.GetLastFocusedViewportIndex();
        FocusedState = &Layout.GetViewportState(FocusedIdx);
        FocusedClient = Layout.GetViewportClient(FocusedIdx);
    }

    ImGui::Text("View");
    if (FocusedState)
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
        };

        ImGui::SetNextItemWidth(ItemWidth);
        if (ImGui::BeginCombo("View Mode", GetViewModeName(FocusedState->ViewMode)))
        {
            for (EViewMode Mode : Modes)
            {
                const bool bSelected = FocusedState->ViewMode == Mode;
                if (ImGui::Selectable(GetViewModeName(Mode), bSelected))
                {
                    FocusedState->ViewMode = Mode;
                }
                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SetNextItemWidth(ItemWidth);
        if (ImGui::BeginCombo("Light Culling", GetLightCullModeName(FocusedState->LightCullMode)))
        {
            static constexpr ELightCullMode CullModes[] = {
                ELightCullMode::Clustered,
                ELightCullMode::Tiled,
                ELightCullMode::None,
            };
            for (ELightCullMode CullMode : CullModes)
            {
                const bool bSelected = FocusedState->LightCullMode == CullMode;
                if (ImGui::Selectable(GetLightCullModeName(CullMode), bSelected))
                {
                    FocusedState->LightCullMode = CullMode;
                }
                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }
    else
    {
        ImGui::TextDisabled("No focused viewport");
    }

    ImGui::SetNextItemWidth(ItemWidth);
    if (ImGui::BeginCombo("Shadow Filter", GetShadowFilterName(Settings.ShadowFilterMode)))
    {
        if (ImGui::Selectable("PCF", Settings.ShadowFilterMode == EShadowFilter::PCF))
        {
            Settings.ShadowFilterMode = EShadowFilter::PCF;
        }
        if (ImGui::Selectable("VSM", Settings.ShadowFilterMode == EShadowFilter::VSM))
        {
            Settings.ShadowFilterMode = EShadowFilter::VSM;
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    // Show Flags
    ImGui::Text("Show");
    ImGui::Checkbox("Primitives", &Settings.ShowFlags.bPrimitives);
    ImGui::Checkbox("BillboardText", &Settings.ShowFlags.bBillboardText);
	ImGui::Checkbox("Axis", &Settings.ShowFlags.bAxis);
    ImGui::Checkbox("Grid", &Settings.ShowFlags.bGrid);
    ImGui::Checkbox("Gizmo", &Settings.ShowFlags.bGizmo);
    ImGui::Checkbox("Bounding Volume", &Settings.ShowFlags.bBoundingVolume);
    if (Settings.ShowFlags.bBoundingVolume)
    {
        ImGui::Indent();
        ImGui::Checkbox("BVH Bounding Volume", &Settings.ShowFlags.bBVHBoundingVolume);
        ImGui::Unindent();
    }
	ImGui::Checkbox("Enable LOD", &Settings.ShowFlags.bEnableLOD);
    ImGui::Checkbox("Decals", &Settings.ShowFlags.bDecals);
    ImGui::Checkbox("Fog", &Settings.ShowFlags.bFog);
    ImGui::Checkbox("Shadow", &Settings.ShowFlags.bShadow);
    ImGui::Checkbox("Gamma Correction", &Settings.ShowFlags.bGammaCorrection);
    if (Settings.ShowFlags.bGammaCorrection)
    {
        ImGui::Indent();
        ImGui::SetNextItemWidth(ItemWidth);
        ImGui::SliderFloat("Gamma", &Settings.ShowFlags.GammaValue, 1.0f, 3.0f, "%.2f");
        ImGui::Unindent();
    }

    ImGui::Separator();

    // Grid Settings
    ImGui::Text("Grid");
    
    ImGui::SetNextItemWidth(ItemWidth); // 너비 설정
    ImGui::SliderFloat("Spacing", &Settings.GridSpacing, 0.1f, 10.0f, "%.1f");
    
    ImGui::SetNextItemWidth(ItemWidth); // 너비 설정
    ImGui::SliderInt("Half Line Count", &Settings.GridHalfLineCount, 10, 500);

    ImGui::Separator();
    ImGui::Text("Post Process");
    ImGui::Checkbox("Enable FXAA", &Settings.bEnableFXAA);

    ImGui::Separator();

    // Camera
    ImGui::Text("Camera");

    ImGui::SetNextItemWidth(ItemWidth); // 너비 설정
    ImGui::SliderFloat("Base Speed", &Settings.CameraSpeed, 0.1f, 100.0f, "%.1f");
    
    ImGui::SetNextItemWidth(ItemWidth); // 너비 설정
    ImGui::SliderFloat("Rotate Speed", &Settings.CameraRotationSpeed, 1.0f, 720.0f, "%.0f");

    ImGui::SetNextItemWidth(ItemWidth);
    ImGui::SliderFloat("Dolly Scale", &Settings.CameraDollySpeedScale, 0.05f, 5.0f, "%.2fx");

    ImGui::SetNextItemWidth(ItemWidth);
    ImGui::SliderFloat("Pan Scale", &Settings.CameraPanSpeedScale, 0.05f, 10.0f, "%.2fx");

    ImGui::Checkbox("Smoothing", &Settings.bEnableCameraSmoothing);
    ImGui::BeginDisabled(!Settings.bEnableCameraSmoothing);
    ImGui::SetNextItemWidth(ItemWidth);
    ImGui::SliderFloat("Move Smooth", &Settings.CameraMoveSmoothSpeed, 0.1f, 40.0f, "%.2f");
    ImGui::SetNextItemWidth(ItemWidth);
    ImGui::SliderFloat("Rotate Smooth", &Settings.CameraRotateSmoothSpeed, 0.1f, 40.0f, "%.2f");
    ImGui::EndDisabled();

    if (FocusedClient)
    {
        float CameraSpeedMultiplier = GetCameraSpeedMultiplier(FocusedClient);

        ImGui::SetNextItemWidth(ItemWidth); // 너비 설정
        if (ImGui::SliderFloat("Speed Multiplier", &CameraSpeedMultiplier, 0.01f, 500.0f, "%.2fx"))
        {
            SetCameraSpeedMultiplier(FocusedClient, CameraSpeedMultiplier);
        }
    }

    ImGui::Separator();

    ImGui::Text("BVH Maintenance");
    bool bPolicyChanged = false;

    ImGui::SetNextItemWidth(ItemWidth);
    bPolicyChanged |= ImGui::SliderInt("Batch Refit Min Dirty", &Settings.SpatialBatchRefitMinDirtyCount, 1, 256);

    ImGui::SetNextItemWidth(ItemWidth);
    bPolicyChanged |= ImGui::SliderInt("Batch Refit Dirty %", &Settings.SpatialBatchRefitDirtyPercentThreshold, 1, 100);

    ImGui::SetNextItemWidth(ItemWidth);
    bPolicyChanged |= ImGui::SliderInt("Rotation Structural Changes", &Settings.SpatialRotationStructuralChangeThreshold, 1, 256);

    ImGui::SetNextItemWidth(ItemWidth);
    bPolicyChanged |= ImGui::SliderInt("Rotation Dirty Count", &Settings.SpatialRotationDirtyCountThreshold, 1, 512);

    ImGui::SetNextItemWidth(ItemWidth);
    bPolicyChanged |= ImGui::SliderInt("Rotation Dirty %", &Settings.SpatialRotationDirtyPercentThreshold, 1, 100);

    if (bPolicyChanged && EditorEngine)
    {
        EditorEngine->ApplySpatialIndexMaintenanceSettings();
    }

    RenderShadowAtlasPreview();
    RenderShadowCubeArrayPreview();

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void FEditorViewportOverlayWidget::RenderShadowCubeArrayPreview()
{
    FShadowAtlasManager& AtlasManager = FShadowAtlasManager::Get();
    const uint32 AllocatedCubeCount = AtlasManager.GetAllocatedCubeCount();

    ImGui::Separator();
    if (!ImGui::CollapsingHeader("Point Shadow Cube Array", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    ImGui::TextColored(ImVec4(0.72f, 0.78f, 0.86f, 1.0f),
        "Allocated Cubes: %u / %d    Face: %u x %u",
        AllocatedCubeCount,
        MAX_SHADOW_CUBES,
        SHADOW_CUBE_SIZE,
        SHADOW_CUBE_SIZE);

    if (AllocatedCubeCount == 0)
    {
        ImGui::TextDisabled("No point shadow cube allocated this frame.");
        return;
    }

    static const char* FaceNames[CUBE_FACE_COUNT] =
    {
        "+X", "-X", "+Y", "-Y", "+Z", "-Z"
    };

    static const ImVec4 FaceColors[CUBE_FACE_COUNT] =
    {
        ImVec4(0.95f, 0.35f, 0.32f, 1.0f),
        ImVec4(0.75f, 0.18f, 0.18f, 1.0f),
        ImVec4(0.34f, 0.78f, 0.40f, 1.0f),
        ImVec4(0.16f, 0.54f, 0.25f, 1.0f),
        ImVec4(0.36f, 0.56f, 0.96f, 1.0f),
        ImVec4(0.16f, 0.28f, 0.70f, 1.0f)
    };

    const float AvailableWidth = ImGui::GetContentRegionAvail().x;
    const float Padding = 8.0f;
    const float Gap = 6.0f;
    const float HeaderHeight = 24.0f;
    const float FaceSize = ClampFloat((AvailableWidth - Padding * 2.0f - Gap * 2.0f) / 3.0f, 48.0f, 96.0f);
    const float PanelWidth = FaceSize * 3.0f + Gap * 2.0f + Padding * 2.0f;
    const float PanelHeight = HeaderHeight + FaceSize * 2.0f + Gap + Padding * 2.0f;

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    for (uint32 CubeIndex = 0; CubeIndex < AllocatedCubeCount && CubeIndex < MAX_SHADOW_CUBES; ++CubeIndex)
    {
        ImGui::PushID(static_cast<int>(CubeIndex));

        const ImVec2 PanelMin = ImGui::GetCursorScreenPos();
        const ImVec2 PanelMax = Add(PanelMin, ImVec2(PanelWidth, PanelHeight));
        DrawList->AddRectFilled(PanelMin, PanelMax, ToU32(ImVec4(0.055f, 0.060f, 0.070f, 0.96f)), 6.0f);
        DrawList->AddRect(PanelMin, PanelMax, ToU32(ImVec4(0.24f, 0.28f, 0.34f, 1.0f)), 6.0f);

        ImGui::SetCursorScreenPos(Add(PanelMin, ImVec2(Padding, 5.0f)));
        ImGui::TextColored(ImVec4(0.90f, 0.93f, 0.98f, 1.0f), "Cube %u", CubeIndex);

        for (int Face = 0; Face < CUBE_FACE_COUNT; ++Face)
        {
            const int Column = Face % 3;
            const int Row = Face / 3;
            const ImVec2 CellPos = Add(
                PanelMin,
                ImVec2(
                    Padding + Column * (FaceSize + Gap),
                    HeaderHeight + Row * (FaceSize + Gap)));

            ImGui::SetCursorScreenPos(CellPos);

            ID3D11ShaderResourceView* SRV = AtlasManager.GetCubeDebugSRV(static_cast<int32>(CubeIndex), Face);
            if (SRV != nullptr)
            {
                ImGui::Image(SRV, ImVec2(FaceSize, FaceSize));
            }
            else
            {
                ImGui::InvisibleButton("MissingFace", ImVec2(FaceSize, FaceSize));
            }

            const ImVec2 ImageMin = ImGui::GetItemRectMin();
            const ImVec2 ImageMax = ImGui::GetItemRectMax();
            DrawList->AddRectFilled(
                ImageMin,
                Add(ImageMin, ImVec2(24.0f, 16.0f)),
                ToU32(ImVec4(0.02f, 0.025f, 0.030f, 0.82f)),
                3.0f);
            DrawList->AddText(Add(ImageMin, ImVec2(4.0f, 1.0f)), ToU32(FaceColors[Face]), FaceNames[Face]);
            DrawList->AddRect(ImageMin, ImageMax, ToU32(FaceColors[Face]), 3.0f, 0, 1.5f);
        }
		ImGui::SetCursorScreenPos(PanelMin);
        ImGui::Dummy(ImVec2(PanelWidth, PanelHeight + 6.0f));

        ImGui::PopID();
    }
}

void FEditorViewportOverlayWidget::RenderDebugStats(float DeltaTime)
{
	if (!EditorEngine) return;

	constexpr ImGuiWindowFlags kFlags =
		ImGuiWindowFlags_NoDecoration      |
		ImGuiWindowFlags_AlwaysAutoResize  |
		ImGuiWindowFlags_NoSavedSettings   |
		ImGuiWindowFlags_NoFocusOnAppearing|
		ImGuiWindowFlags_NoNav             |
		ImGuiWindowFlags_NoMove            |
		ImGuiWindowFlags_NoInputs;

	FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
	const FEditorRenderPipeline* RenderPipeline = EditorEngine->GetEditorRenderPipeline();

	for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
	{
		const FEditorViewportState& VS = Layout.GetViewportState(i);
        FViewportRect ViewportRect = Layout.GetSceneViewport(i).GetRect();

		if (!VS.bShowStatFPS && !VS.bShowStatMemory && !VS.bShowStatNameTable && !VS.bShowLight&& !VS.bShowShadow) continue;
        if (ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
            continue; // 비활성 뷰포트 스킵

		// 툴바 바로 아래 좌측에 고정
		ImGui::SetNextWindowPos(
            ImVec2(static_cast<float>(ViewportRect.X) + 8.f,
                   static_cast<float>(ViewportRect.Y) + 32.f),
			ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.3f);

		char WinId[32];
		snprintf(WinId, sizeof(WinId), "##StatOverlay_%d", i);

		if (ImGui::Begin(WinId, nullptr, kFlags))
		{
			const FRenderCollector::FCullingStats* CullingStats =
				(RenderPipeline != nullptr) ? &RenderPipeline->GetViewportCullingStats(i) : nullptr;

			// FPS 출력 (초록색 텍스트)
			if (VS.bShowStatFPS)
			{
				const float FPS = (DeltaTime > 0.f) ? (1.f / DeltaTime) : 0.f;
				ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "FPS: %.1f (%.2f ms)", FPS, DeltaTime * 1000.f);
			}

			if (CullingStats != nullptr)
			{
				const int32 CulledPrimitiveCount = std::max(
					0,
					CullingStats->TotalVisiblePrimitiveCount -
						(CullingStats->BVHPassedPrimitiveCount + CullingStats->FallbackPassedPrimitiveCount));

				if (VS.bShowStatFPS)
				{
					ImGui::Separator();
				}

				ImGui::TextColored(ImVec4(0.25f, 0.9f, 1.0f, 1.0f), "Culling");
				ImGui::TextColored(
					ImVec4(0.25f, 0.9f, 1.0f, 1.0f), "- Total Visible: %d", CullingStats->TotalVisiblePrimitiveCount);
				ImGui::TextColored(
					ImVec4(0.25f, 0.9f, 1.0f, 1.0f), "- BVH Passed: %d", CullingStats->BVHPassedPrimitiveCount);
				ImGui::TextColored(
					ImVec4(0.25f, 0.9f, 1.0f, 1.0f), "- Fallback Passed: %d",
					CullingStats->FallbackPassedPrimitiveCount);
				ImGui::TextColored(ImVec4(0.25f, 0.9f, 1.0f, 1.0f), "- Culled: %d", CulledPrimitiveCount);
			}

			const FRenderCollector::FDecalStats* DecalStats =
				(RenderPipeline != nullptr) ? &RenderPipeline->GetViewportDecalStats(i) : nullptr;

			if (DecalStats != nullptr)
			{
				if (CullingStats != nullptr || VS.bShowStatFPS)
				{
					ImGui::Separator();
				}

				ImGui::TextColored(ImVec4(1.f, 0.5f, 0.f, 1.f), "Decal");
				ImGui::TextColored(ImVec4(1.f, 0.5f, 0.f, 1.f), "- Total Decals: %d", DecalStats->TotalDecalCount);
				ImGui::TextColored(ImVec4(1.f, 0.5f, 0.f, 1.f), "- Decal Time: %.4f ms", DecalStats->CollectTimeMS / 1000.f);
			}

            const FRenderCollector::FLightStats* LightStats =
                (RenderPipeline != nullptr) ? &RenderPipeline->GetViewportLightStats(i) : nullptr;

			if (VS.bShowLight)
            {
				if (LightStats != nullptr)
				{
                    ImGui::Separator();

					ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), "Light");
					ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), "- Total Lights: %d", LightStats->TotalLightCount);
					ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), "- Directional Lights: %d", LightStats->DirectionalLightCount);
					ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), "- Point Lights: %d", LightStats->PointLightCount);
					ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), "- Spot Lights: %d", LightStats->SpotlightCount);
                    ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), "- Shadow Cast Lights: %d", LightStats->ShadowCastingLightCount);
				}
			}

            if (VS.bShowShadow)
            {
                if (CullingStats != nullptr || VS.bShowStatFPS || VS.bShowStatMemory)
                {
                    ImGui::Separator();
                }

                FShadowAtlasManager& ShadowAtlasManager = FShadowAtlasManager::Get();
                const TArray<FShadowAtlasTile> AllocatedTiles = ShadowAtlasManager.GetAllocatedTiles();

                int32 UsedShadowTiles = 0;
                uint64 UsedAtlasPixels = 0;
                for (const FShadowAtlasTile& Tile : AllocatedTiles)
                {
                    if (!Tile.bUsed)
                    {
                        continue;
                    }

                    ++UsedShadowTiles;
                    UsedAtlasPixels += static_cast<uint64>(Tile.Width) * static_cast<uint64>(Tile.Height);
                }

                const uint32 AtlasResolution = ShadowAtlasManager.GetAtlasResolution();
                const uint64 AtlasPixels = static_cast<uint64>(AtlasResolution) * static_cast<uint64>(AtlasResolution);
                const uint64 AtlasBytes = AtlasPixels * sizeof(float);
                const uint64 UsedAtlasBytes = UsedAtlasPixels * sizeof(float);

                const uint32 AllocatedCubeCount = ShadowAtlasManager.GetAllocatedCubeCount();
                const uint64 CubeFacePixels = static_cast<uint64>(SHADOW_CUBE_SIZE) * static_cast<uint64>(SHADOW_CUBE_SIZE);
                const uint64 UsedCubeBytes = static_cast<uint64>(AllocatedCubeCount) * CUBE_FACE_COUNT * CubeFacePixels * sizeof(float);
                const uint64 MaxCubeBytes = static_cast<uint64>(MAX_SHADOW_CUBES) * CUBE_FACE_COUNT * CubeFacePixels * sizeof(float);

                const float AtlasUsagePercent = AtlasPixels > 0
                                                    ? (static_cast<float>(UsedAtlasPixels) / static_cast<float>(AtlasPixels)) * 100.0f
                                                    : 0.0f;

                ImGui::TextColored(ImVec4(0.f, 0.5f, 1.0f, 1.f), "Shadow Stat");
                ImGui::TextColored(ImVec4(0.f, 0.5f, 1.0f, 1.f), "- Atlas Resolution: %u x %u", AtlasResolution, AtlasResolution);
                ImGui::TextColored(ImVec4(0.f, 0.5f, 1.0f, 1.f), "- Used 2D Tiles: %d", UsedShadowTiles);
                ImGui::TextColored(ImVec4(0.f, 0.5f, 1.0f, 1.f), "- Atlas Usage: %.2f%%", AtlasUsagePercent);
                ImGui::TextColored(ImVec4(0.f, 0.5f, 1.0f, 1.f), "- Atlas Memory: %.2f / %.2f MB",
                                   UsedAtlasBytes / (1024.0f * 1024.0f),
                                   AtlasBytes / (1024.0f * 1024.0f));
                ImGui::TextColored(ImVec4(0.f, 0.5f, 1.0f, 1.f), "- Point Cubes: %u / %d", AllocatedCubeCount, MAX_SHADOW_CUBES);
                ImGui::TextColored(ImVec4(0.f, 0.5f, 1.0f, 1.f), "- Cube Memory: %.2f / %.2f MB",
                                   UsedCubeBytes / (1024.0f * 1024.0f),
                                   MaxCubeBytes / (1024.0f * 1024.0f));
            }

			// Memory 출력 (노란색 텍스트)
			if (VS.bShowStatMemory)
			{
				if (CullingStats != nullptr || VS.bShowStatFPS)
				{
					ImGui::Separator();
				}

				size_t MeshMemoryBytes = 0;
				for (TObjectIterator<UStaticMesh> It; It; ++It)
				{
					UStaticMesh* Mesh = *It;
					if (Mesh && Mesh->HasValidMeshData())
					{
						MeshMemoryBytes += sizeof(UStaticMesh)
							+ Mesh->GetVertices().size()  * sizeof(FNormalVertex)
							+ Mesh->GetIndices().size()   * sizeof(uint32)
							+ Mesh->GetSections().size()  * sizeof(FStaticMeshSection);
					}
				}

				const size_t MatMemoryBytes   = FResourceManager::Get().GetMaterialMemorySize();
				const size_t TotalMemoryBytes = MeshMemoryBytes + MatMemoryBytes;

				ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), "Memory Stat");
				ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), "- Mesh: %.2f KB",     MeshMemoryBytes / 1024.f);
				ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), "- Material: %.2f KB", MatMemoryBytes  / 1024.f);
				ImGui::Separator();

				FNamePool& Pool = FNamePool::Get();
				ImGui::TextColored(ImVec4(1.f, 0.5f, 0.5f, 1.f), "FName Stat");
				ImGui::TextColored(ImVec4(1.f, 0.5f, 0.5f, 1.f), "- Entries: %u",   Pool.GetEntryCount());
				ImGui::TextColored(ImVec4(1.f, 0.5f, 0.5f, 1.f), "- Size: %.2f KB", Pool.GetTotalBytes() / 1024.f);

				ImGui::Separator();

				ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 1.f), "- Total Allocated Counts: %d", EngineStatics::GetTotalAllocationCount());
				ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 1.f), "- Total Allocated Bytes: %.2f KB", EngineStatics::GetTotalAllocationBytes() / 1024.f);
			}
		}

		ImGui::End();
	}
}

void FEditorViewportOverlayWidget::RenderGroupedStatOverlay(float DeltaTime)
{
    if (!bShowGroupedStatOverlay || !EditorEngine)
    {
        return;
    }

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    const int32 FocusedIdx = Layout.GetLastFocusedViewportIndex();
    const FViewportRect ViewportRect = Layout.GetSceneViewport(FocusedIdx).GetRect();
    if (ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
    {
        return;
    }

    const FEditorRenderPipeline* RenderPipeline = EditorEngine->GetEditorRenderPipeline();
    const FRenderCollector::FCullingStats* CullingStats =
        RenderPipeline != nullptr ? &RenderPipeline->GetViewportCullingStats(FocusedIdx) : nullptr;
    const FRenderCollector::FDecalStats* DecalStats =
        RenderPipeline != nullptr ? &RenderPipeline->GetViewportDecalStats(FocusedIdx) : nullptr;
    const FRenderCollector::FLightStats* LightStats =
        RenderPipeline != nullptr ? &RenderPipeline->GetViewportLightStats(FocusedIdx) : nullptr;

    constexpr float PaneToolbarHeight = 34.0f;
    constexpr float OverlayMarginX = 12.0f;
    constexpr float OverlayMarginY = 15.0f;
    ImGui::SetNextWindowPos(
        ImVec2(
            static_cast<float>(ViewportRect.X) + OverlayMarginX,
            static_cast<float>(ViewportRect.Y) + PaneToolbarHeight + OverlayMarginY),
        ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.78f);

    constexpr ImGuiWindowFlags Flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.09f, 0.12f, 0.82f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.38f, 0.58f, 0.84f, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.94f, 1.00f, 1.00f));

    if (ImGui::Begin("##Week06GroupedStatOverlay", nullptr, Flags))
    {
        const float FPS = DeltaTime > 0.0f ? 1.0f / DeltaTime : 0.0f;
        ImGui::Text("FPS : %.1f (%.2f ms)", FPS, DeltaTime * 1000.0f);

        if (CullingStats != nullptr)
        {
            const int32 CulledPrimitiveCount = std::max(
                0,
                CullingStats->TotalVisiblePrimitiveCount
                    - (CullingStats->BVHPassedPrimitiveCount + CullingStats->FallbackPassedPrimitiveCount));

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextUnformatted("[ Culling ]");
            ImGui::Text("Total Visible : %d", CullingStats->TotalVisiblePrimitiveCount);
            ImGui::Text("BVH Passed    : %d", CullingStats->BVHPassedPrimitiveCount);
            ImGui::Text("Fallback      : %d", CullingStats->FallbackPassedPrimitiveCount);
            ImGui::Text("Culled        : %d", CulledPrimitiveCount);
        }

        if (DecalStats != nullptr)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextUnformatted("[ Decal ]");
            ImGui::Text("Total Decals : %d", DecalStats->TotalDecalCount);
            ImGui::Text("Collect Time : %.4f ms", DecalStats->CollectTimeMS / 1000.0f);
        }

        if (LightStats != nullptr)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextUnformatted("[ Light ]");
            ImGui::Text("Total Lights      : %d", LightStats->TotalLightCount);
            ImGui::Text("Directional       : %d", LightStats->DirectionalLightCount);
            ImGui::Text("Point             : %d", LightStats->PointLightCount);
            ImGui::Text("Spot              : %d", LightStats->SpotlightCount);
            ImGui::Text("Shadow Cast       : %d", LightStats->ShadowCastingLightCount);
        }

        FShadowAtlasManager& ShadowAtlasManager = FShadowAtlasManager::Get();
        const TArray<FShadowAtlasTile> AllocatedTiles = ShadowAtlasManager.GetAllocatedTiles();
        int32 UsedShadowTiles = 0;
        uint64 UsedAtlasPixels = 0;
        for (const FShadowAtlasTile& Tile : AllocatedTiles)
        {
            if (!Tile.bUsed)
            {
                continue;
            }
            ++UsedShadowTiles;
            UsedAtlasPixels += static_cast<uint64>(Tile.Width) * static_cast<uint64>(Tile.Height);
        }

        const uint32 AtlasResolution = ShadowAtlasManager.GetAtlasResolution();
        const uint64 AtlasPixels = static_cast<uint64>(AtlasResolution) * static_cast<uint64>(AtlasResolution);
        const float AtlasUsagePercent =
            AtlasPixels > 0
                ? (static_cast<float>(UsedAtlasPixels) / static_cast<float>(AtlasPixels)) * 100.0f
                : 0.0f;

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("[ Shadow ]");
        ImGui::Text("Atlas       : %u x %u", AtlasResolution, AtlasResolution);
        ImGui::Text("Used Tiles  : %d", UsedShadowTiles);
        ImGui::Text("Atlas Usage : %.2f%%", AtlasUsagePercent);
        ImGui::Text("Point Cubes : %u / %d", ShadowAtlasManager.GetAllocatedCubeCount(), MAX_SHADOW_CUBES);

        size_t MeshMemoryBytes = 0;
        for (TObjectIterator<UStaticMesh> It; It; ++It)
        {
            UStaticMesh* Mesh = *It;
            if (Mesh && Mesh->HasValidMeshData())
            {
                MeshMemoryBytes += sizeof(UStaticMesh)
                    + Mesh->GetVertices().size() * sizeof(FNormalVertex)
                    + Mesh->GetIndices().size() * sizeof(uint32)
                    + Mesh->GetSections().size() * sizeof(FStaticMeshSection);
            }
        }

        const size_t MaterialMemoryBytes = FResourceManager::Get().GetMaterialMemorySize();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("[ Memory ]");
        ImGui::Text("Mesh     : %.2f KB", MeshMemoryBytes / 1024.0f);
        ImGui::Text("Material : %.2f KB", MaterialMemoryBytes / 1024.0f);
        ImGui::Text("FName    : %.2f KB", FNamePool::Get().GetTotalBytes() / 1024.0f);
        ImGui::Text("Objects  : %d / %.2f KB",
            EngineStatics::GetTotalAllocationCount(),
            EngineStatics::GetTotalAllocationBytes() / 1024.0f);
    }
    ImGui::End();

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
}

// 스플리터 바 시각화
void FEditorViewportOverlayWidget::RenderSplitterBar(ImDrawList* DrawList)
{
	if (!DrawList)
	{
		return;
	}

	 // Capture 중이거나 middle dragging 중이라면 하이라이트를 하지 않습니다.
	if (FSlateApplication::Get().GetCapturedWidget() || InputSystem::Get().GetMiddleDragging())
		 return;
	// 우클릭 + 기즈모 홀딩 중에는 하이라이트를 표시하지 않음
    const FWorldContext* Ctx = EditorEngine->GetFocusedWorldContext();
	bool bIsHodingGizmo = Ctx->SelectionManager->GetGizmo()->IsHolding();

	 if (bIsHodingGizmo || InputSystem::Get().GetRightDragging())
	 {
		 return;
	 }

	 if (!EditorEngine) return;
	
	FEditorViewportLayout& ViewportLayout = EditorEngine->GetViewportLayout();
	if (ViewportLayout.IsLayoutTransitionActive())
	{
		return;
	}

	// 1개 모드일 때는 바를 그리지 않음
	if (ViewportLayout.GetLayoutMode() == EEditorViewportLayoutMode::FourPanes2x2)
	{
		constexpr ImU32 BarColor = IM_COL32(80, 80, 80, 220);
		constexpr ImU32 HoverColor = IM_COL32(140, 180, 255, 255);

		const SWidget* Hovered  = FSlateApplication::Get().GetHoveredWidget();
		const SWidget* Captured = FSlateApplication::Get().GetCapturedWidget();

		const bool bIsDragging = InputSystem::Get().GetRightDragging();

		SSplitterCross* Cross = ViewportLayout.GetCrossWidget();
		constexpr ImU32 CrossHoverColor = IM_COL32(140, 180, 255, 255);

		const bool bCrossHovered = (Cross && Cross == Hovered);

		SSplitter* Splitters[] = {
			ViewportLayout.GetRootSplitterV(),
			ViewportLayout.GetTopSplitterH(),
			ViewportLayout.GetBotSplitterH()
		};

		for (SSplitter* S : Splitters)
		{
			if (!S) continue;
			const FRect Bar = S->GetBarRect();

			const SSplitter* Linked = S->GetLinkedSplitter();
			const bool bSplitterHover = !bIsDragging
				&& ((S == Hovered || S == Captured)
					|| (Linked && (Linked == Hovered || Linked == Captured)));

			ImU32 Color = BarColor;
			if (bCrossHovered)       Color = CrossHoverColor;
			else if (bSplitterHover) Color = HoverColor;

			DrawList->AddRectFilled(
				ImVec2(Bar.X, Bar.Y),
				ImVec2(Bar.X + Bar.Width, Bar.Y + Bar.Height),
				Color);
		}

		// 교차점 핸들 렌더링 (4개 뷰포트 동시 조정)
		if (Cross)
		{
			const FRect CR = Cross->GetCrossRect();
			DrawList->AddRectFilled(
				ImVec2(CR.X, CR.Y),
				ImVec2(CR.X + CR.Width, CR.Y + CR.Height),
				bCrossHovered ? CrossHoverColor : BarColor);
		}
	}
}

void FEditorViewportOverlayWidget::RenderViewportFocusOverlay()
{
	if (!EditorEngine)
	{
		return;
	}

	FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const FViewportRect& HostRect = Layout.GetHostRect();
	const ImVec2 OverlayPos = (HostRect.Width > 0 && HostRect.Height > 0)
		? ImVec2(static_cast<float>(HostRect.X), static_cast<float>(HostRect.Y))
		: (MainViewport ? MainViewport->WorkPos : ImVec2(0.0f, 0.0f));
	const ImVec2 OverlaySize = (HostRect.Width > 0 && HostRect.Height > 0)
		? ImVec2(static_cast<float>(HostRect.Width), static_cast<float>(HostRect.Height))
		: (MainViewport ? MainViewport->WorkSize : ImGui::GetIO().DisplaySize);

	ImGui::SetNextWindowPos(OverlayPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(OverlaySize, ImGuiCond_Always);
	if (MainViewport)
	{
		ImGui::SetNextWindowViewport(MainViewport->ID);
	}
	constexpr ImGuiWindowFlags OverlayFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoBringToFrontOnFocus;
	ImGui::Begin("##ViewportFocusOverlayUnderPanels", nullptr, OverlayFlags);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const int32 FocusedIndex = Layout.GetLastFocusedViewportIndex();

	for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
	{
		const FViewportRect ViewportRect = Layout.GetSceneViewport(i).GetRect();
		if (ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
		{
			continue;
		}

		const FEditorViewportState& State = Layout.GetViewportState(i);
		const FEditorViewportClient* Client = Layout.GetViewportClient(i);
		const float PIEFlashAlpha = Client ? Client->GetPIEStartOutlineFlashAlpha() : 0.0f;
		const bool bFocused = (i == FocusedIndex);
		const bool bHovered = State.bHovered;
		if (!bFocused && !bHovered && PIEFlashAlpha <= 0.0f)
		{
			continue;
		}

		const ImVec2 Min(static_cast<float>(ViewportRect.X), static_cast<float>(ViewportRect.Y));
		const ImVec2 Max(
			static_cast<float>(ViewportRect.X + ViewportRect.Width),
			static_cast<float>(ViewportRect.Y + ViewportRect.Height));
		const ImU32 Color = bFocused ? IM_COL32(82, 168, 255, 235) : IM_COL32(170, 190, 210, 120);
		const float Thickness = bFocused ? 2.0f : 1.0f;
		DrawList->AddRect(Min, Max, Color, 0.0f, 0, Thickness);
		if (PIEFlashAlpha > 0.0f)
		{
			const int32 Alpha = static_cast<int32>(220.0f * PIEFlashAlpha);
			DrawList->AddRect(Min, Max, IM_COL32(120, 255, 150, Alpha), 0.0f, 0, 4.0f);
		}
	}
	ImGui::End();
}

void FEditorViewportOverlayWidget::RenderBoxSelectionOverlay()
{
	if (!EditorEngine)
	{
		return;
	}

	FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
	ImDrawList* DrawList = ImGui::GetForegroundDrawList();
	const bool bAdditive = InputSystem::Get().GetKey(VK_SHIFT);
	const ImU32 RectColor = bAdditive ? IM_COL32(128, 240, 128, 220) : IM_COL32(128, 192, 255, 220);
	const ImU32 FillColor = bAdditive ? IM_COL32(64, 180, 64, 40) : IM_COL32(64, 128, 220, 40);

	for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
	{
		const FEditorViewportState& VS = Layout.GetViewportState(i);
        FViewportRect ViewportRect = Layout.GetSceneViewport(i).GetRect();
        if (ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
		{
			continue;
		}

		const FEditorViewportClient* Client = Layout.GetViewportClient(i);
		if (!Client->IsBoxSelecting())
		{
			continue;
		}

		const POINT Start = Client->GetBoxSelectStart();
		const POINT End = Client->GetBoxSelectEnd();

		const float MinX = static_cast<float>(std::min(Start.x, End.x));
		const float MinY = static_cast<float>(std::min(Start.y, End.y));
		const float MaxX = static_cast<float>(std::max(Start.x, End.x));
		const float MaxY = static_cast<float>(std::max(Start.y, End.y));

		const ImVec2 P0(static_cast<float>(ViewportRect.X) + MinX, static_cast<float>(ViewportRect.Y) + MinY);
        const ImVec2 P1(static_cast<float>(ViewportRect.X) + MaxX, static_cast<float>(ViewportRect.Y) + MaxY);
		DrawList->AddRectFilled(P0, P1, FillColor);
		DrawList->AddRect(P0, P1, RectColor, 0.0f, 0, 1.5f);
	}
}

void FEditorViewportOverlayWidget::RenderShortcutsWindow()
{
	if (!bShowShortcutsWindow)
	{
		return;
	}

	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const ImVec2 OverlayPos = MainViewport ? MainViewport->Pos : ImVec2(0.0f, 0.0f);
	const ImVec2 OverlaySize = MainViewport ? MainViewport->Size : ImGui::GetIO().DisplaySize;

	ImGui::SetNextWindowPos(OverlayPos);
	ImGui::SetNextWindowSize(OverlaySize);
	ImGui::SetNextWindowBgAlpha(0.38f);
	const ImGuiWindowFlags BlockerFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoMove;
	ImGui::Begin("##ShortcutOverlayBlocker", nullptr, BlockerFlags);
	ImGui::InvisibleButton("##ShortcutOverlayBlockerBtn", OverlaySize);
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) ||
		ImGui::IsItemClicked(ImGuiMouseButton_Right) ||
		ImGui::IsItemClicked(ImGuiMouseButton_Middle))
	{
		ImGui::SetWindowFocus("Shortcuts");
	}
	ImGui::End();

	const ImVec2 PanelSize(980.0f, 700.0f);
	ImGui::SetNextWindowPos(
		ImVec2(
			OverlayPos.x + (OverlaySize.x - PanelSize.x) * 0.5f,
			OverlayPos.y + (OverlaySize.y - PanelSize.y) * 0.5f),
		ImGuiCond_Always);
	ImGui::SetNextWindowSize(PanelSize, ImGuiCond_Always);

	if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
	{
		bShowShortcutsWindow = false;
		return;
	}

	ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.14f, 0.15f, 0.18f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.16f, 0.17f, 0.20f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.28f, 0.29f, 0.32f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::SetNextWindowFocus();

	const ImGuiWindowFlags PanelFlags =
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove;

	bool bOpen = bShowShortcutsWindow;
	if (!ImGui::Begin("Shortcuts", &bOpen, PanelFlags))
	{
		ImGui::End();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(3);
		bShowShortcutsWindow = bOpen;
		return;
	}
	bShowShortcutsWindow = bOpen;

	ImGui::TextUnformatted("현재 코드상 실제로 동작하는 에디터 단축키를 정리했습니다.");
	ImGui::Separator();

	constexpr float ShortcutColumnWidth = 200.0f;
	auto DrawShortcutSection = [ShortcutColumnWidth](const char* SectionName, const char* TableId, std::initializer_list<std::pair<const char*, const char*>> Rows)
	{
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.14f, 0.23f, 0.47f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f, 0.91f, 0.98f, 1.0f));
		ImGui::Selectable(SectionName, false, ImGuiSelectableFlags_Disabled, ImVec2(-1.0f, 22.0f));
		ImGui::PopStyleColor(2);

		if (ImGui::BeginTable(
			TableId,
			2,
			ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadOuterX))
		{
			ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, ShortcutColumnWidth);
			ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("Shortcut");
			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted("Action");

			for (const auto& Row : Rows)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(Row.first);
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(Row.second);
			}

			ImGui::EndTable();
		}
		ImGui::Spacing();
	};

	DrawShortcutSection("▼ Viewport Navigation", "ShortcutTable_Nav",
	{
		{"Mouse Left Drag", "좌우 카메라 회전 / 상하 돌리 이동"},
		{"Mouse Right Drag", "뷰포트 카메라 회전 (Perspective)"},
		{"Mouse Middle Drag", "뷰포트 카메라 팬 이동"},
		{"Alt + Mouse Left Drag", "선택 대상을 기준으로 오빗 회전"},
		{"Alt + Mouse Right Drag", "카메라 돌리 인/아웃"},
		{"Alt + Mouse Middle Drag", "카메라 팬 이동"},
		{"Mouse Wheel", "Perspective 돌리 이동 또는 직교 카메라 줌 조절"},
		{"Right Mouse + Wheel", "카메라 이동 속도 배율 조절"},
		{"W / A / S / D / Q / E", "카메라 이동 (뷰포트 내비게이션 중일 때만 적용)"},
		{"Arrow Keys", "카메라 회전 (뷰포트 내비게이션 중일 때만 적용)"},
		{"F", "현재 선택된 Actor 축으로 카메라 포커스"},
	});

	DrawShortcutSection("▼ Selection", "ShortcutTable_Selection",
	{
		{"Mouse Left Click", "Actor 단일 선택"},
		{"Shift + Mouse Left Click", "선택 추가"},
		{"Ctrl + Mouse Left Click", "선택 토글"},
		{"Ctrl + Alt + Drag", "박스 선택"},
		{"Ctrl + Alt + Shift + Drag", "기존 선택에 박스 선택 추가"},
		{"Ctrl + A", "전체 Actor 선택"},
	});

	DrawShortcutSection("▼ Gizmo", "ShortcutTable_Gizmo",
	{
		{"Mouse Left Drag", "기즈모 축 드래그 조작"},
		{"Q / W / E / R", "Select / Translate / Rotate / Scale 모드 전환"},
		{"1 / 2 / 3 / 4", "Select / Translate / Rotate / Scale 모드 전환"},
		{"Tab", "Editor Mode 순환"},
		{"Space", "기즈모 타입 순환"},
		{"X", "월드/로컬 기즈모 모드 전환"},
		{"Viewport Snap Buttons", "이동 / 회전 / 스케일 스냅 토글 및 값 선택"},
	});

	DrawShortcutSection("▼ File", "ShortcutTable_File",
	{
		{"Ctrl + N", "New Scene"},
		{"Ctrl + O", "Load Scene"},
		{"Ctrl + S", "Save Scene"},
		{"Ctrl + Shift + S", "Save Scene As"},
	});

	DrawShortcutSection("▼ Editor", "ShortcutTable_Editor",
	{
		{"Ctrl + Z", "Undo"},
		{"Ctrl + Shift + Z", "Redo"},
		{"Delete", "선택된 Actor 삭제"},
		{"Ctrl + D", "선택된 Actor 복제"},
		{"Backtick(`)", "Console Mode 순환"},
		{"Ctrl + Space", "Content Browser 열기 / 닫기"},
		{"Viewport Toolbar", "Type / Cam / View / Stats / Settings / Layout / Split 조작"},
	});

	DrawShortcutSection("▼ PIE", "ShortcutTable_PIE",
	{
		{"Esc", "PIE 종료"},
		{"F8", "Possess / Eject 토글"},
		{"Shift + F1", "마우스 캡처 해제"},
		{"W / A / S / D", "PIE 플레이어 이동"},
		{"Mouse Move (captured)", "PIE 플레이어 카메라 회전"},
	});

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextUnformatted("참고: ImGui 입력창이 키보드를 잡고 있으면 일부 단축키는 동작하지 않을 수 있습니다.");
	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);
}


void FEditorViewportOverlayWidget::RenderShadowAtlasPreview()
{
#if STATS
    if (!ImGui::CollapsingHeader("Shadow Atlas", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    ID3D11ShaderResourceView* ShadowAtlasSRV = FShadowAtlasManager::Get().GetSRV();
    if (ShadowAtlasSRV == nullptr)
    {
        ImGui::TextDisabled("Shadow atlas SRV is not available.");
        return;
    }

    ImGui::PushID("ShadowAtlasPreview");

    ImGui::Checkbox("Grid", &bShowShadowAtlasGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Zoom", &bShowShadowAtlasZoom);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    ImGui::SliderFloat("Size", &ShadowAtlasPreviewSize, 192.0f, 640.0f, "%.0f px");

    const float AvailableWidth = ImGui::GetContentRegionAvail().x;
    const float PreviewSize = ClampFloat(ShadowAtlasPreviewSize, 128.0f, std::max(128.0f, AvailableWidth));
    const ImVec2 ImageSize(PreviewSize, PreviewSize);
    const ImVec2 ImagePos = ImGui::GetCursorScreenPos();
    const ImVec2 ImageMax = Add(ImagePos, ImageSize);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const ImU32 BackColor = IM_COL32(18, 20, 24, 255);
    const ImU32 BorderColor = IM_COL32(110, 130, 155, 210);
    const ImU32 GridMajorColor = IM_COL32(255, 255, 255, 65);
    const ImU32 GridMinorColor = IM_COL32(255, 255, 255, 25);
    const ImU32 HoverColor = IM_COL32(80, 190, 255, 230);

    DrawList->AddRectFilled(ImagePos, ImageMax, BackColor, 4.0f);
    ImGui::Image(ShadowAtlasSRV, ImageSize);

    DrawList->AddRectFilled(ImagePos, ImageMax, IM_COL32(4, 8, 12, 140), 4.0f);
    if (bShowShadowAtlasGrid)
    {
        const float GridCount = static_cast<float>(ShadowAtlasResolution2D) / AtlasGridCellPixels;
        for (int i = 1; i < static_cast<int>(GridCount); ++i)
        {
            const float T = static_cast<float>(i) / GridCount;
            const float X = ImagePos.x + ImageSize.x * T;
            const float Y = ImagePos.y + ImageSize.y * T;
            const bool bMajor = (i % 4) == 0;
            DrawList->AddLine(ImVec2(X, ImagePos.y), ImVec2(X, ImageMax.y), bMajor ? GridMajorColor : GridMinorColor, bMajor ? 1.2f : 1.0f);
            DrawList->AddLine(ImVec2(ImagePos.x, Y), ImVec2(ImageMax.x, Y), bMajor ? GridMajorColor : GridMinorColor, bMajor ? 1.2f : 1.0f);
        }
    }

    const TArray<FShadowAtlasTile> AtlasTiles = FShadowAtlasManager::Get().GetAllocatedTiles();
    static const ImU32 TileColors[] =
    {
        IM_COL32(255, 104, 104, 255),
        IM_COL32(255, 190,  82, 255),
        IM_COL32(105, 219, 124, 255),
        IM_COL32( 88, 191, 255, 255),
        IM_COL32(164, 132, 255, 255),
        IM_COL32(255, 117, 214, 255),
        IM_COL32(117, 232, 217, 255),
        IM_COL32(230, 232, 117, 255)
    };

    int32 UsedTileCount = 0;
    int32 HoveredTileIndex = -1;
    const FShadowAtlasTile* HoveredTile = nullptr;
    for (const FShadowAtlasTile& Tile : AtlasTiles)
    {
        if (!Tile.bUsed)
        {
            continue;
        }

        const ImVec2 TileMin = AtlasPixelToPreview(ImagePos, ImageSize, Tile.X, Tile.Y);
        const ImVec2 TileMax = AtlasPixelToPreview(ImagePos, ImageSize, Tile.X + Tile.Width, Tile.Y + Tile.Height);
        const ImU32 TileColor = TileColors[UsedTileCount % (sizeof(TileColors) / sizeof(TileColors[0]))];

        DrawList->AddRectFilled(TileMin, TileMax, IM_COL32(0, 0, 0, 42), 2.0f);
        DrawList->AddRect(TileMin, TileMax, TileColor, 2.0f, 0, 2.0f);

        const ImVec2 TileSize = Subtract(TileMax, TileMin);
        if (TileSize.x >= 22.0f && TileSize.y >= 18.0f)
        {
            char Label[16];
            snprintf(Label, sizeof(Label), "#%d", UsedTileCount);
            DrawList->AddRectFilled(
                TileMin,
                Add(TileMin, ImVec2(24.0f, 17.0f)),
                IM_COL32(3, 6, 10, 205),
                2.0f);
            DrawList->AddText(Add(TileMin, ImVec2(4.0f, 1.0f)), TileColor, Label);
        }

        if (ImGui::IsMouseHoveringRect(TileMin, TileMax))
        {
            HoveredTileIndex = UsedTileCount;
            HoveredTile = &Tile;
        }

        ++UsedTileCount;
    }

    DrawList->AddRect(ImagePos, ImageMax, BorderColor, 4.0f, 0, 1.5f);

    if (ImGui::IsItemHovered())
    {
        const ImVec2 Mouse = ImGui::GetIO().MousePos;
        const ImVec2 Local = Subtract(Mouse, ImagePos);
        const float U = ClampFloat(Local.x / ImageSize.x, 0.0f, 1.0f);
        const float V = ClampFloat(Local.y / ImageSize.y, 0.0f, 1.0f);
        const int PixelX = static_cast<int>(U * static_cast<float>(ShadowAtlasResolution2D));
        const int PixelY = static_cast<int>(V * static_cast<float>(ShadowAtlasResolution2D));
        const int TileX = static_cast<int>(PixelX / static_cast<int>(AtlasGridCellPixels));
        const int TileY = static_cast<int>(PixelY / static_cast<int>(AtlasGridCellPixels));

        DrawList->AddLine(ImVec2(Mouse.x, ImagePos.y), ImVec2(Mouse.x, ImageMax.y), HoverColor, 1.0f);
        DrawList->AddLine(ImVec2(ImagePos.x, Mouse.y), ImVec2(ImageMax.x, Mouse.y), HoverColor, 1.0f);

        const float TileSizeOnPreview = ImageSize.x * (AtlasGridCellPixels / static_cast<float>(ShadowAtlasResolution2D));
        const ImVec2 TileMin(
            ImagePos.x + static_cast<float>(TileX) * TileSizeOnPreview,
            ImagePos.y + static_cast<float>(TileY) * TileSizeOnPreview);
        DrawList->AddRect(TileMin, Add(TileMin, ImVec2(TileSizeOnPreview, TileSizeOnPreview)), HoverColor, 0.0f, 0, 1.5f);

        if (bShowShadowAtlasZoom && ImGui::BeginTooltip())
        {
            const float HalfRegion = AtlasZoomRegionUv * 0.5f;
            const ImVec2 Uv0(
                ClampFloat(U - HalfRegion, 0.0f, 1.0f),
                ClampFloat(V - HalfRegion, 0.0f, 1.0f));
            const ImVec2 Uv1(
                ClampFloat(U + HalfRegion, 0.0f, 1.0f),
                ClampFloat(V + HalfRegion, 0.0f, 1.0f));

            ImGui::Text("UV %.4f, %.4f", U, V);
            ImGui::Text("Pixel %d, %d", PixelX, PixelY);
            ImGui::Text("Grid %d, %d", TileX, TileY);
            if (HoveredTile != nullptr)
            {
                ImGui::Separator();
                ImGui::Text("Shadow #%d", HoveredTileIndex);
                ImGui::Text("Rect %d, %d  %dx%d",
                    HoveredTile->X,
                    HoveredTile->Y,
                    HoveredTile->Width,
                    HoveredTile->Height);
                ImGui::Text("Tree depth %d", HoveredTile->Depth);
            }
            ImGui::Separator();
            ImGui::Image(ShadowAtlasSRV, ImVec2(220.0f, 220.0f), Uv0, Uv1);
            ImGui::EndTooltip();
        }
    }

    ImGui::TextDisabled(
        "Atlas %ux%u | shadows %d | grid %.0f px | hover shows pixel, grid cell, and zoom",
        ShadowAtlasResolution2D,
        ShadowAtlasResolution2D,
        UsedTileCount,
        AtlasGridCellPixels);

    ImGui::PopID();
#endif
}
