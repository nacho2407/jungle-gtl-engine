#include "Editor/UI/EditorViewportOverlayWidget.h"

#include "Editor/Settings/EditorSettings.h"
#include "ImGui/imgui.h"

#include "Engine/Object/ObjectIterator.h"
#include "Engine/Asset/StaticMesh.h"
#include "Engine/Asset/StaticMeshTypes.h"
#include <cstdio>

void FEditorViewportOverlayWidget::Render(float DeltaTime)
{
	RenderViewportSettings(DeltaTime);
	RenderDebugStats(DeltaTime);
}

void FEditorViewportOverlayWidget::RenderViewportSettings(float DeltaTime)
{
	FEditorSettings& Settings = FEditorSettings::Get();

	if (!ImGui::Begin("Viewport Settings"))
	{
		ImGui::End();
		return;
	}

	// View Mode
	ImGui::Text("View Mode");
	int32 CurrentMode = static_cast<int32>(Settings.ViewMode);
	ImGui::RadioButton("Lit", &CurrentMode, static_cast<int32>(EViewMode::Lit));
	ImGui::SameLine();
	ImGui::RadioButton("Unlit", &CurrentMode, static_cast<int32>(EViewMode::Unlit));
	ImGui::SameLine();
	ImGui::RadioButton("Wireframe", &CurrentMode, static_cast<int32>(EViewMode::Wireframe));
	Settings.ViewMode = static_cast<EViewMode>(CurrentMode);

	ImGui::Separator();

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
	FEditorSettings& Settings = FEditorSettings::Get();

	ImGuiViewport* Viewport = ImGui::GetMainViewport();
	ImVec2 WindowPos(Viewport->WorkPos.x + (Viewport->WorkSize.x * 0.5f), Viewport->WorkPos.y + 10.0f);
	// 둘 다 꺼져있으면 렌더링하지 않음
	if (!Settings.bShowStatFPS && !Settings.bShowStatMemory)
	{
		return;
	}

	ImGui::SetNextWindowPos(WindowPos, ImGuiCond_Always, ImVec2(0.5f, 0.0f));
	ImGui::SetNextWindowBgAlpha(0.3f); // 30% 반투명한 검은색 배경

	// 오버레이 창을 위한 플래그: 테두리/타이틀바 없음, 자동 크기, 이동 불가, 클릭 무시(NoInputs)
	ImGuiWindowFlags Flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoInputs; // 마우스 클릭이 뷰포트를 통과하도록 설정

	if (ImGui::Begin("##StatOverlay", nullptr, Flags))
	{
		// FPS 출력 (초록색 텍스트)
		if (Settings.bShowStatFPS)
		{
			float FPS = (DeltaTime > 0.0f) ? (1.0f / DeltaTime) : 0.0f;
			float MS = DeltaTime * 1000.0f;
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "FPS: %.1f (%.2f ms)", FPS, MS);
		}

		// Memory 출력 (노란색 텍스트)
		if (Settings.bShowStatMemory)
		{
			size_t TotalMemoryBytes = 0;

			// 이전에 작성했던 UStaticMesh 순회 로직 활용
			for (TObjectIterator<UStaticMesh> It; It; ++It)
			{
				UStaticMesh* Mesh = *It;
				if (Mesh && Mesh->HasValidMeshData())
				{
					size_t VerticesMem = Mesh->GetVertices().size() * sizeof(FNormalVertex);
					size_t IndicesMem = Mesh->GetIndices().size() * sizeof(uint32);
					size_t SectionsMem = Mesh->GetSections().size() * sizeof(FStaticMeshSection);
					
					TotalMemoryBytes += sizeof(UStaticMesh) + VerticesMem + IndicesMem + SectionsMem;
				}
			}

			float MemoryKB = TotalMemoryBytes / (1024.0f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Mesh Memory: %.2f KB", MemoryKB);
		}
	}

	ImGui::End();
}