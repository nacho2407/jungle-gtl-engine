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
#include <cstdio>
#include "Slate/SSplitter.h"
#include "Slate/SSplitterV.h"
#include "Slate/SSplitterH.h"
#include "Slate/SSplitterCross.h"
#include "Viewport/ViewportLayout.h"
#include "Input/InputSystem.h"
#include <initializer_list>
#include <utility>
#include <algorithm>
#include "Engine/Component/GizmoComponent.h"

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
	case EViewMode::Lit:       return "Lit";
	case EViewMode::Unlit:     return "Unlit";
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

    ImGui::Separator();

    // Grid Settings
    ImGui::Text("Grid");
    
    ImGui::SetNextItemWidth(ItemWidth); // 너비 설정
    ImGui::SliderFloat("Spacing", &Settings.GridSpacing, 0.1f, 10.0f, "%.1f");
    
    ImGui::SetNextItemWidth(ItemWidth); // 너비 설정
    ImGui::SliderInt("Half Line Count", &Settings.GridHalfLineCount, 10, 500);

    ImGui::Separator();
    ImGui::Text("Post Process");
    ImGui::SetNextItemWidth(ItemWidth);
    ImGui::SliderFloat("FXAA Threshold", &Settings.FXAAThreshold, 0.0f, 1.0f, "%.3f");

    ImGui::Separator();

    // Camera Sensitivity
    ImGui::Text("Camera");

    ImGui::SetNextItemWidth(ItemWidth); // 너비 설정
    ImGui::SliderFloat("Move Sensitivity", &Settings.CameraMoveSensitivity, 0.1f, 5.0f, "%.1f");
    
    ImGui::SetNextItemWidth(ItemWidth); // 너비 설정
    ImGui::SliderFloat("Rotate Sensitivity", &Settings.CameraRotateSensitivity, 0.1f, 5.0f, "%.1f");

    if (EditorEngine)
    {
        FViewportLayout& Layout = EditorEngine->GetViewportLayout();
        const int32 FocusedIdx = Layout.GetLastFocusedViewportIndex();
        FEditorViewportClient& FocusedClient = Layout.GetViewportClient(FocusedIdx);
        float CameraMoveSpeed = FocusedClient.GetMoveSpeed();

        ImGui::SetNextItemWidth(ItemWidth); // 너비 설정
        if (ImGui::SliderFloat("Dolly Speed", &CameraMoveSpeed, 10.0f, 2000.0f, "%.0f"))
        {
            FocusedClient.SetMoveSpeed(CameraMoveSpeed);
        }
    }

    ImGui::Separator();

    ImGui::Text("BVH Maintenance");
    bool bPolicyChanged = false;

    ImGui::SetNextItemWidth(ItemWidth);
    bPolicyChanged |= ImGui::SliderInt("Batch Refit Min Dirty", &Settings.SpatialBatchRefitMinDirtyCount, 1, 256);

    ImGui::SetNextItemWidth(ItemWidth);
    bPolicyChanged |= ImGui::SliderInt("Batch Refit Dirty %%", &Settings.SpatialBatchRefitDirtyPercentThreshold, 1, 100);

    ImGui::SetNextItemWidth(ItemWidth);
    bPolicyChanged |= ImGui::SliderInt("Rotation Structural Changes", &Settings.SpatialRotationStructuralChangeThreshold, 1, 256);

    ImGui::SetNextItemWidth(ItemWidth);
    bPolicyChanged |= ImGui::SliderInt("Rotation Dirty Count", &Settings.SpatialRotationDirtyCountThreshold, 1, 512);

    ImGui::SetNextItemWidth(ItemWidth);
    bPolicyChanged |= ImGui::SliderInt("Rotation Dirty %%", &Settings.SpatialRotationDirtyPercentThreshold, 1, 100);

    if (bPolicyChanged && EditorEngine)
    {
        EditorEngine->ApplySpatialIndexMaintenanceSettings();
    }

    ImGui::End();
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

	FViewportLayout& Layout = EditorEngine->GetViewportLayout();
	const FEditorRenderPipeline* RenderPipeline = EditorEngine->GetEditorRenderPipeline();

	for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
	{
		const FEditorViewportState& VS = Layout.GetViewportState(i);

		if (!VS.bShowStatFPS && !VS.bShowStatMemory) continue;
		if (VS.Rect.Width <= 0 || VS.Rect.Height <= 0) continue; // 비활성 뷰포트 스킵

		// 툴바 바로 아래 좌측에 고정
		ImGui::SetNextWindowPos(
			ImVec2(static_cast<float>(VS.Rect.X) + 8.f,
			       static_cast<float>(VS.Rect.Y) + 32.f),
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
				
				ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), "- Total Allocated Counts: %d", EngineStatics::GetTotalAllocationCount());
				ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), "- Total Allocated Bytes: %.2f KB", EngineStatics::GetTotalAllocationBytes() / 1024.f);
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
	
	FViewportLayout& ViewportLayout = EditorEngine->GetViewportLayout();

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

	FViewportLayout& Layout = EditorEngine->GetViewportLayout();
	ImDrawList* DrawList = ImGui::GetForegroundDrawList();
	const bool bAdditive = InputSystem::Get().GetKey(VK_SHIFT);
	const ImU32 RectColor = bAdditive ? IM_COL32(128, 240, 128, 220) : IM_COL32(128, 192, 255, 220);
	const ImU32 FillColor = bAdditive ? IM_COL32(64, 180, 64, 40) : IM_COL32(64, 128, 220, 40);

	for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
	{
		const FEditorViewportState& VS = Layout.GetViewportState(i);
		if (VS.Rect.Width <= 0 || VS.Rect.Height <= 0)
		{
			continue;
		}

		const FEditorViewportClient& Client = Layout.GetViewportClient(i);
		if (!Client.IsBoxSelecting())
		{
			continue;
		}

		const POINT Start = Client.GetBoxSelectStart();
		const POINT End = Client.GetBoxSelectEnd();

		const float MinX = static_cast<float>(std::min(Start.x, End.x));
		const float MinY = static_cast<float>(std::min(Start.y, End.y));
		const float MaxX = static_cast<float>(std::max(Start.x, End.x));
		const float MaxY = static_cast<float>(std::max(Start.y, End.y));

		const ImVec2 P0(static_cast<float>(VS.Rect.X) + MinX, static_cast<float>(VS.Rect.Y) + MinY);
		const ImVec2 P1(static_cast<float>(VS.Rect.X) + MaxX, static_cast<float>(VS.Rect.Y) + MaxY);
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
