#include "Editor/UI/EditorShadowMapDebugWidget.h"
#include "Editor/EditorEngine.h"
#include "Runtime/Engine.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/Resource/RenderResources.h"
#include "Render/RenderPass/ShadowMapPass.h"
#include "ImGui/imgui.h"

static const char* FaceNames[] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };

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
	ImGui::RadioButton("Point (t23)", &SelectedTab, 2);
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

		// Depth brightness: reversed-Z atlas tends to be near-black without a boost
		ImGui::SetNextItemWidth(180.0f);
		ImGui::SliderFloat("Brightness", &SpotDepthBrightness, 0.1f, 8.0f, "%.2fx");
		ImGui::SameLine();
		if (ImGui::SmallButton("Reset")) SpotDepthBrightness = 1.0f;

		ImGui::Checkbox("Show Spot Regions", &bShowSpotRegions);

		// 선택된 slice 프리뷰
		if (SpotPageIndex >= 0 && SpotPageIndex < (int32)SR.SpotAtlasPageCount && SR.SpotAtlasSliceSRVs && SR.SpotAtlasSliceSRVs[SpotPageIndex])
		{
			float B = SpotDepthBrightness;
			ImGui::Image(
				(ImTextureID)SR.SpotAtlasSliceSRVs[SpotPageIndex],
				ImVec2(PreviewSize, PreviewSize),
				ImVec2(0, 0), ImVec2(1, 1),
				ImVec4(B, B, B, 1.0f), ImVec4(0.3f, 0.3f, 0.3f, 1.0f)
			);

			// Colored per-light region overlay
			if (bShowSpotRegions)
			{
				FShadowMapPass* ShadowPass = Renderer.GetPipeline().FindPass<FShadowMapPass>();
				if (ShadowPass)
				{
					const TArray<FAtlasRegion>& Regions = ShadowPass->GetLastSpotAtlasRegions();

					ImVec2 ImageMin = ImGui::GetItemRectMin();
					float  Scale    = PreviewSize / static_cast<float>(SR.SpotAtlasResolution);

					static const ImU32 RegionColors[] = {
						IM_COL32(255,  80,  80, 220),
						IM_COL32( 80, 220,  80, 220),
						IM_COL32( 80, 140, 255, 220),
						IM_COL32(255, 220,  50, 220),
						IM_COL32(255, 130,  30, 220),
						IM_COL32(200,  80, 255, 220),
						IM_COL32( 50, 230, 230, 220),
						IM_COL32(255,  80, 180, 220),
					};
					constexpr int32 NumColors = (int32)(sizeof(RegionColors) / sizeof(RegionColors[0]));

					ImDrawList* DrawList = ImGui::GetWindowDrawList();

					for (int32 i = 0; i < (int32)Regions.size(); ++i)
					{
						const FAtlasRegion& R = Regions[i];
						if (!R.bValid) continue;

						ImU32  Col     = RegionColors[i % NumColors];
						ImVec2 RMin    = ImVec2(ImageMin.x + R.X          * Scale, ImageMin.y + R.Y          * Scale);
						ImVec2 RMax    = ImVec2(ImageMin.x + (R.X + R.Size) * Scale, ImageMin.y + (R.Y + R.Size) * Scale);

						DrawList->AddRect(RMin, RMax, Col, 0.0f, 0, 2.0f);

						char Lbl[16];
						snprintf(Lbl, sizeof(Lbl), "L%d (%upx)", i, R.Size);
						DrawList->AddText(ImVec2(RMin.x + 4.0f, RMin.y + 4.0f), Col, Lbl);
					}
				}
			}
		}
	}
	// ════════════════════════════════════════
	// Point Atlas (t23)
	// ════════════════════════════════════════
	else if (SelectedTab == 2)
	{
		if (!SR.IsPointLightValid())
		{
			ImGui::TextDisabled("Point Atlas: not allocated");
			ImGui::End();
			return;
		}

		ImGui::Text("Atlas: %u x %u", SR.PointAtlasResolution, SR.PointAtlasResolution);

		ImGui::SetNextItemWidth(180.0f);
		ImGui::SliderFloat("Brightness##pt", &PointDepthBrightness, 0.1f, 8.0f, "%.2fx");
		ImGui::SameLine();
		if (ImGui::SmallButton("Reset##pt")) PointDepthBrightness = 1.0f;

		ImGui::Checkbox("Show Point Regions", &bShowPointRegions);

		if (SR.PointAtlasSRV)
		{
			float B = PointDepthBrightness;
			ImGui::Image(
				(ImTextureID)SR.PointAtlasSRV,
				ImVec2(PreviewSize, PreviewSize),
				ImVec2(0, 0), ImVec2(1, 1),
				ImVec4(B, B, B, 1.0f), ImVec4(0.3f, 0.3f, 0.3f, 1.0f)
			);

			if (bShowPointRegions)
			{
				FShadowMapPass* ShadowPass = Renderer.GetPipeline().FindPass<FShadowMapPass>();
				if (ShadowPass)
				{
					const TArray<FAtlasRegion>& Regions = ShadowPass->GetLastPointAtlasRegions();

					ImVec2 ImageMin = ImGui::GetItemRectMin();
					float  Scale = PreviewSize / static_cast<float>(SR.SpotAtlasResolution);

					static const ImU32 RegionColors[] = {
						IM_COL32(255,  80,  80, 220),
						IM_COL32(80, 220,  80, 220),
						IM_COL32(80, 140, 255, 220),
						IM_COL32(255, 220,  50, 220),
						IM_COL32(255, 130,  30, 220),
						IM_COL32(200,  80, 255, 220),
						IM_COL32(50, 230, 230, 220),
						IM_COL32(255,  80, 180, 220),
					};
					constexpr int32 NumColors = (int32)(sizeof(RegionColors) / sizeof(RegionColors[0]));

					ImDrawList* DrawList = ImGui::GetWindowDrawList();

					for (int32 i = 0; i < (int32)Regions.size(); ++i)
					{
						const FAtlasRegion& R = Regions[i];
						if (!R.bValid) continue;

						ImU32  Col = RegionColors[i % NumColors];
						ImVec2 RMin = ImVec2(ImageMin.x + R.X * Scale, ImageMin.y + R.Y * Scale);
						ImVec2 RMax = ImVec2(ImageMin.x + (R.X + R.Size) * Scale, ImageMin.y + (R.Y + R.Size) * Scale);

						DrawList->AddRect(RMin, RMax, Col, 0.0f, 0, 2.0f);

						char Lbl[16];
						snprintf(Lbl, sizeof(Lbl), "L%d (%upx)", i, R.Size);
						DrawList->AddText(ImVec2(RMin.x + 4.0f, RMin.y + 4.0f), Col, Lbl);
					}
				}
			}
		}
	}

	ImGui::End();
}
