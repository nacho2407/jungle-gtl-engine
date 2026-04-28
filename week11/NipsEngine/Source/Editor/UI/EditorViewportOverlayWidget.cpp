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
	default:                   return "Lit";
	}
}

void FEditorViewportOverlayWidget::Render(float DeltaTime)
{
	if (bShowViewportSettings)
	{
		RenderViewportSettings(DeltaTime);
	}
	RenderDebugStats(DeltaTime);
	RenderSplitterBar();
	RenderBoxSelectionOverlay();
    RenderShortcutsWindow();
}

void FEditorViewportOverlayWidget::RenderViewportSettings(float DeltaTime)
{
    FEditorSettings& Settings = FEditorSettings::Get();

    if (!ImGui::Begin("Viewport Settings"))
    {
        ImGui::End();
        return;
    }

    // 위젯 너비를 현재 창 콘텐츠 영역의 50%로 설정하는 람다 또는 변수
    float ItemWidth = ImGui::GetContentRegionAvail().x * 0.5f;

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

    // Camera Sensitivity
    ImGui::Text("Camera");

    ImGui::SetNextItemWidth(ItemWidth); // 너비 설정
    ImGui::SliderFloat("Move Sensitivity", &Settings.CameraMoveSensitivity, 0.1f, 5.0f, "%.1f");
    
    ImGui::SetNextItemWidth(ItemWidth); // 너비 설정
    ImGui::SliderFloat("Rotate Sensitivity", &Settings.CameraRotateSensitivity, 0.1f, 5.0f, "%.1f");

    if (EditorEngine)
    {
        FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
        const int32 FocusedIdx = Layout.GetLastFocusedViewportIndex();
        FEditorViewportClient* FocusedClient = Layout.GetViewportClient(FocusedIdx);
        float CameraMoveSpeed = FocusedClient->GetMoveSpeed();

        ImGui::SetNextItemWidth(ItemWidth); // 너비 설정
        if (ImGui::SliderFloat("Dolly Speed", &CameraMoveSpeed, 10.0f, 2000.0f, "%.0f"))
        {
            FocusedClient->SetMoveSpeed(CameraMoveSpeed);
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

		if (!VS.bShowStatFPS && !VS.bShowStatMemory && !VS.bShowStatNameTable) continue;
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

// 스플리터 바 시각화
void FEditorViewportOverlayWidget::RenderSplitterBar()
{

	 // Capture 중이거나 middle dragging 중이라면 하이라이트를 하지 않습니다.
	if (FSlateApplication::Get().GetCapturedWidget() || InputSystem::Get().GetMiddleDragging())
		 return;
	// 우클릭 + 기즈모 홀딩 중에는 하이라이트를 표시하지 않음
	bool bIsHodingGizmo = EditorEngine->GetGizmo()->IsHolding();

	 if (bIsHodingGizmo || InputSystem::Get().GetRightDragging())
	 {
		 return;
	 }

	 if (!EditorEngine) return;
	
	FEditorViewportLayout& ViewportLayout = EditorEngine->GetViewportLayout();

	// 1개 모드일 때는 바를 그리지 않음
	if (!ViewportLayout.IsSingleViewportMode())
	{
		ImDrawList* DrawList = ImGui::GetForegroundDrawList();
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

	ImGui::OpenPopup("Shortcuts##Modal");
	ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, 0.55f));
	ImGui::SetNextWindowSize(ImVec2(760.0f, 560.0f), ImGuiCond_Appearing);

	if (!ImGui::BeginPopupModal("Shortcuts##Modal", &bShowShortcutsWindow, ImGuiWindowFlags_NoResize))
	{
		ImGui::PopStyleColor();
		return;
	}

	if (!bShowShortcutsWindow)
	{
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		ImGui::PopStyleColor();
		return;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
	{
		bShowShortcutsWindow = false;
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		ImGui::PopStyleColor();
		return;
	}

	ImGui::TextUnformatted("Shortcuts");

	ImGui::Separator();
	ImGui::Text("현재 코드상 실제로 동작하는 에디터 단축키만 정리했습니다.");

	auto DrawShortcutTable = [](const char* Header, std::initializer_list<std::pair<const char*, const char*>> Rows)
	{
		if (!ImGui::CollapsingHeader(Header, ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		if (ImGui::BeginTable(Header, 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter))
		{
			ImGui::TableSetupColumn("Shortcut");
			ImGui::TableSetupColumn("Action");
			ImGui::TableHeadersRow();

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
	};

	DrawShortcutTable("Viewport Navigation",
	{
		{"Mouse Right Drag", "뷰포트 카메라 회전"},
		{"Mouse Middle Drag", "뷰포트 카메라 팬 이동"},
		{"Alt + Mouse Right Drag", "카메라 돌리 인/아웃"},
		{"Mouse Wheel", "원근 카메라 FOV 또는 직교 카메라 높이 조절"},
		{"Mouse Wheel while rotating", "카메라 이동 속도 조절"},
		{"W / A / S / D / Q / E", "카메라 이동 (회전 중일 때만 적용)"},
		{"F", "현재 선택된 Actor 쪽으로 카메라 포커스"},
	});

	DrawShortcutTable("Selection",
	{
		{"Mouse Left Click", "Actor 단일 선택"},
		{"Shift + Mouse Left Click", "선택 추가"},
		{"Ctrl + Mouse Left Click", "선택 토글"},
		{"Ctrl + A", "전체 Actor 선택"},
		{"Ctrl + Alt + Drag", "박스 선택"},
		{"Ctrl + Alt + Shift + Drag", "기존 선택에 박스 선택 추가"},
	});

	DrawShortcutTable("Gizmo",
	{
		{"Mouse Left Drag", "기즈모 축 드래그 조작"},
		{"Space", "기즈모 타입 순환"},
		{"X", "월드/로컬 기즈모 모드 전환"},
	});

	DrawShortcutTable("Editor",
	{
		{"Delete", "선택된 Actor 삭제"},
	});

	ImGui::Spacing();
	ImGui::TextUnformatted("참고: ImGui 입력창이 키보드를 잡고 있을 때는 일부 단축키가 동작하지 않습니다.");
	ImGui::EndPopup();
	ImGui::PopStyleColor();
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
