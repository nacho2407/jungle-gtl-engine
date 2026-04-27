#include "Editor/UI/EditorShadowMapDebugWidget.h"
#include "Editor/EditorEngine.h"
#include "Runtime/Engine.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/Resource/RenderResources.h"
#include "ImGui/imgui.h"

static const char* CubeFaceNames[] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };

void EditorShadowMapDebugWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!ImGui::Begin("Shadow Map Debug"))
	{
		ImGui::End();
		return;
	}

	FRenderer& Renderer = GEngine->GetRenderer();
	const FShadowMapResources& SR = Renderer.GetResources().ShadowResources;

	// ── 상단 라디오 버튼: t21 / t22 / t23 ──
	ImGui::RadioButton("CSM (t21)", &SelectedTab, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Spot Atlas (t22)", &SelectedTab, 1);
	ImGui::SameLine();
	ImGui::RadioButton("Point Cube (t23)", &SelectedTab, 2);
	ImGui::Separator();

	float AvailWidth = ImGui::GetContentRegionAvail().x;
	float PreviewSize = (AvailWidth > 0.0f) ? AvailWidth : 256.0f;

	// ════════════════════════════════════════
	// CSM (t21)
	// ════════════════════════════════════════
	if (SelectedTab == 0)
	{
		if (!SR.IsCSMValid())
		{
			ImGui::TextDisabled("CSM: not allocated");
			ImGui::End();
			return;
		}

		ImGui::Text("Resolution: %u x %u", SR.CSMResolution, SR.CSMResolution);
		ImGui::Text("C%d range: %.3f - %.3f",
			CSMCascadeIndex,
			SR.CSMDebugCascadeNear.Data[CSMCascadeIndex],
			SR.CSMDebugCascadeFar.Data[CSMCascadeIndex]);

		// Cascade 선택 버튼
		for (int32 i = 0; i < (int32)MAX_SHADOW_CASCADES; ++i)
		{
			if (i > 0) ImGui::SameLine();
			char label[16];
			snprintf(label, sizeof(label), "C%d", i);
			if (ImGui::RadioButton(label, &CSMCascadeIndex, i)) {}
		}

		// 선택된 cascade 프리뷰
		if (CSMCascadeIndex >= 0 && CSMCascadeIndex < (int32)MAX_SHADOW_CASCADES && SR.CSMSliceSRV[CSMCascadeIndex])
		{
			ImGui::Image(
				(ImTextureID)SR.CSMSliceSRV[CSMCascadeIndex],
				ImVec2(PreviewSize, PreviewSize),
				ImVec2(0, 0), ImVec2(1, 1),
				ImVec4(1, 1, 1, 1), ImVec4(0.3f, 0.3f, 0.3f, 1)
			);
		}
	}
	// ════════════════════════════════════════
	// Spot Atlas (t22)
	// ════════════════════════════════════════
	else if (SelectedTab == 1)
	{
		if (!SR.IsSpotValid())
		{
			ImGui::TextDisabled("Spot Atlas: not allocated");
			ImGui::End();
			return;
		}

		ImGui::Text("Resolution: %u x %u, Pages: %u", SR.SpotAtlasResolution, SR.SpotAtlasResolution, SR.SpotAtlasPageCount);

		// Page 선택
		if (SpotPageIndex >= (int32)SR.SpotAtlasPageCount)
			SpotPageIndex = 0;

		for (int32 i = 0; i < (int32)SR.SpotAtlasPageCount; ++i)
		{
			if (i > 0) ImGui::SameLine();
			char label[16];
			snprintf(label, sizeof(label), "Page %d", i);
			ImGui::RadioButton(label, &SpotPageIndex, i);
		}

		// 선택된 slice 프리뷰
		if (SpotPageIndex >= 0 && SpotPageIndex < (int32)SR.SpotAtlasPageCount && SR.SpotAtlasSliceSRVs && SR.SpotAtlasSliceSRVs[SpotPageIndex])
		{
			ImGui::Image(
				(ImTextureID)SR.SpotAtlasSliceSRVs[SpotPageIndex],
				ImVec2(PreviewSize, PreviewSize),
				ImVec2(0, 0), ImVec2(1, 1),
				ImVec4(1, 1, 1, 1), ImVec4(0.3f, 0.3f, 0.3f, 1)
			);
		}
	}
	// ════════════════════════════════════════
	// Point CubeMap (t23)
	// ════════════════════════════════════════
	else if (SelectedTab == 2)
	{
		if (!SR.IsPointValid())
		{
			ImGui::TextDisabled("Point CubeMap: not allocated");
			ImGui::End();
			return;
		}

		ImGui::Text("Resolution: %u x %u, Cubes: %u", SR.PointCubeResolution, SR.PointCubeResolution, SR.PointCubeCount);

		// Cube 선택
		if (PointCubeIndex >= (int32)SR.PointCubeCount)
			PointCubeIndex = 0;

		ImGui::SetNextItemWidth(100.0f);
		ImGui::SliderInt("Cube", &PointCubeIndex, 0, (int32)SR.PointCubeCount - 1);

		// Face 선택 (6방향)
		for (int32 i = 0; i < 6; ++i)
		{
			if (i > 0) ImGui::SameLine();
			ImGui::RadioButton(CubeFaceNames[i], &PointFaceIndex, i);
		}

		// TODO: per-face slice SRV 생성 후 여기서 프리뷰
		ImGui::TextDisabled("Cube %d, Face %s preview (TODO: per-face SRV)", PointCubeIndex, CubeFaceNames[PointFaceIndex]);
	}

	ImGui::End();
}
