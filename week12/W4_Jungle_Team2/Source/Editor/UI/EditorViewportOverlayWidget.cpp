#include "Editor/UI/EditorViewportOverlayWidget.h"

#include "Core/ResourceManager.h"
#include "Editor/EditorEngine.h"
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
#include "Viewport/ViewportLayout.h"
#include "Core/InputSystem.h"

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
	RenderViewportSettings(DeltaTime);
	RenderDebugStats(DeltaTime);
	RenderSplitterBar();
}

void FEditorViewportOverlayWidget::RenderViewportSettings(float DeltaTime)
{
	FEditorSettings& Settings = FEditorSettings::Get();

	if (!ImGui::Begin("Viewport Settings"))
	{
		ImGui::End();
		return;
	}

	// Show Flags
	ImGui::Text("Show");
	ImGui::Checkbox("Primitives", &Settings.ShowFlags.bPrimitives);
	ImGui::Checkbox("BillboardText", &Settings.ShowFlags.bBillboardText);
	ImGui::Checkbox("Grid", &Settings.ShowFlags.bGrid);
	ImGui::Checkbox("Gizmo", &Settings.ShowFlags.bGizmo);
	ImGui::Checkbox("Bounding Volume", &Settings.ShowFlags.bBoundingVolume);

	ImGui::Separator();

	// Grid Settings
	ImGui::Text("Grid");
	ImGui::SliderFloat("Spacing", &Settings.GridSpacing, 0.1f, 10.0f, "%.1f");
	ImGui::SliderInt("Half Line Count", &Settings.GridHalfLineCount, 10, 500);

	ImGui::Separator();

	// Camera Sensitivity
	ImGui::Text("Camera");
	ImGui::SliderFloat("Move Sensitivity", &Settings.CameraMoveSensitivity, 0.1f, 5.0f, "%.1f");
	ImGui::SliderFloat("Rotate Sensitivity", &Settings.CameraRotateSensitivity, 0.1f, 5.0f, "%.1f");

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
			// FPS 출력 (초록색 텍스트)
			if (VS.bShowStatFPS)
			{
				const float FPS = (DeltaTime > 0.f) ? (1.f / DeltaTime) : 0.f;
				ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "FPS: %.1f (%.2f ms)", FPS, DeltaTime * 1000.f);
			}

			// Memory 출력 (노란색 텍스트)
			if (VS.bShowStatMemory)
			{
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

void FEditorViewportOverlayWidget::RenderSplitterBar()
{
	// Caputre 중인 Widget이 있다면 SplitterBar 하이라이트를 비활성화 합니다.
	 if (FSlateApplication::Get().GetCapturedWidget()) return;

	// 스플리터 바 시각화
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

		// 드래그 중(LButton + 이동)에는 하이라이트를 표시하지 않음
		// InputSystem::Get().GetLeftDragging() || 
		const bool bIsDragging = InputSystem::Get().GetRightDragging();

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
			const bool bHighlight = !bIsDragging
				&& ((S == Hovered || S == Captured)
					|| (Linked && (Linked == Hovered || Linked == Captured)));

			DrawList->AddRectFilled(
				ImVec2(Bar.X, Bar.Y),
				ImVec2(Bar.X + Bar.Width, Bar.Y + Bar.Height),
				bHighlight ? HoverColor : BarColor);
		}
	}
}
