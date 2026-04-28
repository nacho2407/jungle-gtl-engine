#include "Editor/UI/EditorShadowMapDebugWidget.h"
#include "Editor/EditorEngine.h"
#include "Runtime/Engine.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/Resource/RenderResources.h"
#include "Render/RenderPass/ShadowMapPass.h"
#include "Render/Scene/FScene.h"
#include "Render/Scene/SceneEnvironment.h"
#include "GameFramework/World.h"
#include "Component/Light/SpotLightComponent.h"
#include "Component/Light/PointLightComponent.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "ImGui/imgui.h"

static const char* FaceNames[] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };

// ── Helper: 선택된 Actor에서 LightComponent를 찾고 SceneEnvironment 인덱스 반환 ──

enum class ESelectedLightType { None, Directional, Spot, Point };

struct FSelectedLightInfo
{
	ESelectedLightType Type = ESelectedLightType::None;
	int32 EnvIndex = -1;	// SceneEnvironment 내 인덱스
};

static FSelectedLightInfo FindSelectedLight()
{
	FSelectedLightInfo Info;

	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (!Editor) return Info;

	UWorld* World = GEngine->GetWorld();
	if (!World) return Info;

	const FSceneEnvironment& Env = World->GetScene().GetEnvironment();
	FSelectionManager& Sel = Editor->GetSelectionManager();

	// 1) SelectedComponent 우선 검사
	if (USceneComponent* SelComp = Sel.GetSelectedComponent())
	{
		if (USpotLightComponent* Spot = Cast<USpotLightComponent>(SelComp))
		{
			Info.Type = ESelectedLightType::Spot;
			Info.EnvIndex = Env.FindSpotLightIndex(Spot);
			return Info;
		}
		if (UPointLightComponent* Point = Cast<UPointLightComponent>(SelComp))
		{
			Info.Type = ESelectedLightType::Point;
			Info.EnvIndex = Env.FindPointLightIndex(Point);
			return Info;
		}
	}

	// 2) Actor fallback
	AActor* Selected = Sel.GetPrimarySelection();
	if (!Selected) return Info;

	for (UActorComponent* Comp : Selected->GetComponents())
	{
		if (USpotLightComponent* Spot = Cast<USpotLightComponent>(Comp))
		{
			Info.Type = ESelectedLightType::Spot;
			Info.EnvIndex = Env.FindSpotLightIndex(Spot);
			return Info;
		}
		if (UPointLightComponent* Point = Cast<UPointLightComponent>(Comp))
		{
			Info.Type = ESelectedLightType::Point;
			Info.EnvIndex = Env.FindPointLightIndex(Point);
			return Info;
		}
	}

	if (Selected->IsA<ADirectionalLightActor>())
	{
		Info.Type = ESelectedLightType::Directional;
		Info.EnvIndex = 0;
	}

	return Info;
}

// ── 공용 Region 오버레이 그리기 ──

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
constexpr int32 NumRegionColors = (int32)(sizeof(RegionColors) / sizeof(RegionColors[0]));

static void DrawRegionOverlay(const TArray<FAtlasRegion>& Regions, float PreviewSize, float AtlasResolution, int32 HighlightLightIdx = -1)
{
	ImVec2 ImageMin = ImGui::GetItemRectMin();
	float Scale = PreviewSize / AtlasResolution;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	for (int32 i = 0; i < (int32)Regions.size(); ++i)
	{
		const FAtlasRegion& R = Regions[i];
		if (!R.bValid) continue;

		ImU32  Col  = RegionColors[i % NumRegionColors];
		ImVec2 RMin = ImVec2(ImageMin.x + R.X * Scale, ImageMin.y + R.Y * Scale);
		ImVec2 RMax = ImVec2(ImageMin.x + (R.X + R.Size) * Scale, ImageMin.y + (R.Y + R.Size) * Scale);

		float Thickness = (HighlightLightIdx >= 0 && R.LightIdx == HighlightLightIdx) ? 4.0f : 2.0f;
		DrawList->AddRect(RMin, RMax, Col, 0.0f, 0, Thickness);

		char Lbl[16];
		snprintf(Lbl, sizeof(Lbl), "L%d (%upx)", R.LightIdx, R.Size);
		DrawList->AddText(ImVec2(RMin.x + 4.0f, RMin.y + 4.0f), Col, Lbl);
	}
}

// ============================================================

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
	FShadowMapPass* ShadowPass = Renderer.GetPipeline().FindPass<FShadowMapPass>();

	FSelectedLightInfo SelLight = FindSelectedLight();

	// ── 선택된 라이트가 있으면 자동 탭 전환 ──
	if (SelLight.Type == ESelectedLightType::Directional)
		SelectedTab = 0;
	else if (SelLight.Type == ESelectedLightType::Spot)
		SelectedTab = 1;
	else if (SelLight.Type == ESelectedLightType::Point)
		SelectedTab = 2;

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
		if (!SR.CSM.IsValid())
		{
			ImGui::TextDisabled("CSM: not allocated");
			ImGui::End();
			return;
		}

		ImGui::Text("Resolution: %u x %u", SR.CSM.Resolution, SR.CSM.Resolution);
		ImGui::Text("C%d range: %.3f - %.3f",
			CSMCascadeIndex,
			SR.CSM.DebugCascadeNear.Data[CSMCascadeIndex],
			SR.CSM.DebugCascadeFar.Data[CSMCascadeIndex]);

		// Cascade 선택 버튼
		for (int32 i = 0; i < (int32)MAX_SHADOW_CASCADES; ++i)
		{
			if (i > 0) ImGui::SameLine();
			char label[16];
			snprintf(label, sizeof(label), "C%d", i);
			if (ImGui::RadioButton(label, &CSMCascadeIndex, i)) {}
		}

		// 선택된 cascade 프리뷰
		if (CSMCascadeIndex >= 0 && CSMCascadeIndex < (int32)MAX_SHADOW_CASCADES && SR.CSM.SliceSRV[CSMCascadeIndex])
		{
			ImGui::Image(
				(ImTextureID)SR.CSM.SliceSRV[CSMCascadeIndex],
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
		if (!SR.Spot.IsValid())
		{
			ImGui::TextDisabled("Spot Atlas: not allocated");
			ImGui::End();
			return;
		}

		ImGui::Text("Resolution: %u x %u, Pages: %u", SR.Spot.Resolution, SR.Spot.Resolution, SR.Spot.PageCount);

		// Page 선택
		if (SpotPageIndex >= (int32)SR.Spot.PageCount)
			SpotPageIndex = 0;

		for (int32 i = 0; i < (int32)SR.Spot.PageCount; ++i)
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

		// ── 선택된 SpotLight가 있으면 해당 영역만 crop 표시 ──
		const TArray<FAtlasRegion>* pRegions = ShadowPass ? &ShadowPass->GetLastSpotAtlasRegions() : nullptr;
		bool bShowCropped = false;

		if (SelLight.Type == ESelectedLightType::Spot && SelLight.EnvIndex >= 0 && pRegions)
		{
			for (const FAtlasRegion& R : *pRegions)
			{
				if (!R.bValid || R.LightIdx != SelLight.EnvIndex) continue;

				float AtlasF = static_cast<float>(SR.Spot.Resolution);
				ImVec2 uv0(static_cast<float>(R.X) / AtlasF, static_cast<float>(R.Y) / AtlasF);
				ImVec2 uv1(static_cast<float>(R.X + R.Size) / AtlasF, static_cast<float>(R.Y + R.Size) / AtlasF);

				ImGui::Text("Selected: L%d (%u x %u px)", R.LightIdx, R.Size, R.Size);

				float B = SpotDepthBrightness;
				if (!SR.Spot.SliceSRVs.empty() && SR.Spot.SliceSRVs[SpotPageIndex])
				{
					ImGui::Image(
						(ImTextureID)SR.Spot.SliceSRVs[SpotPageIndex],
						ImVec2(PreviewSize, PreviewSize),
						uv0, uv1,
						ImVec4(B, B, B, 1.0f), ImVec4(0.3f, 0.3f, 0.3f, 1.0f)
					);
				}
				bShowCropped = true;
				break;
			}
		}

		// ── 선택 없거나 못 찾으면 기존: 아틀라스 전체 표시 ──
		if (!bShowCropped)
		{
			if (SpotPageIndex >= 0 && SpotPageIndex < (int32)SR.Spot.PageCount && !SR.Spot.SliceSRVs.empty() && SR.Spot.SliceSRVs[SpotPageIndex])
			{
				float B = SpotDepthBrightness;
				ImGui::Image(
					(ImTextureID)SR.Spot.SliceSRVs[SpotPageIndex],
					ImVec2(PreviewSize, PreviewSize),
					ImVec2(0, 0), ImVec2(1, 1),
					ImVec4(B, B, B, 1.0f), ImVec4(0.3f, 0.3f, 0.3f, 1.0f)
				);

				if (bShowSpotRegions && pRegions)
					DrawRegionOverlay(*pRegions, PreviewSize, static_cast<float>(SR.Spot.Resolution));
			}
		}
	}
	// ════════════════════════════════════════
	// Point Atlas (t23)
	// ════════════════════════════════════════
	else if (SelectedTab == 2)
	{
		if (!SR.Point.IsValid())
		{
			ImGui::TextDisabled("Point Atlas: not allocated");
			ImGui::End();
			return;
		}

		ImGui::Text("Atlas: %u x %u", SR.Point.Resolution, SR.Point.Resolution);

		ImGui::SetNextItemWidth(180.0f);
		ImGui::SliderFloat("Brightness##pt", &PointDepthBrightness, 0.1f, 8.0f, "%.2fx");
		ImGui::SameLine();
		if (ImGui::SmallButton("Reset##pt")) PointDepthBrightness = 1.0f;

		ImGui::Checkbox("Show Point Regions", &bShowPointRegions);

		// ── 선택된 PointLight가 있으면 해당 6 face 영역만 crop 표시 ──
		const TArray<FAtlasRegion>* pRegions = ShadowPass ? &ShadowPass->GetLastPointAtlasRegions() : nullptr;
		bool bShowCropped = false;

		if (SelLight.Type == ESelectedLightType::Point && SelLight.EnvIndex >= 0 && pRegions && SR.Point.SRV)
		{
			// 해당 라이트의 face 영역 수집
			TArray<const FAtlasRegion*> FaceRegions;
			for (const FAtlasRegion& R : *pRegions)
			{
				if (R.bValid && R.LightIdx == SelLight.EnvIndex)
					FaceRegions.push_back(&R);
			}

			if (!FaceRegions.empty())
			{
				bShowCropped = true;
				uint32 FaceSize = FaceRegions.empty() ? 0 : FaceRegions[0]->Size;
				ImGui::Text("Selected: L%d (%u x %u px, %u faces)", SelLight.EnvIndex, FaceSize, FaceSize, (uint32)FaceRegions.size());

				float FacePreview = (PreviewSize - 10.0f * 2) / 3.0f;
				float AtlasF = static_cast<float>(SR.Point.Resolution);
				float B = PointDepthBrightness;

				for (int32 f = 0; f < (int32)FaceRegions.size(); ++f)
				{
					const FAtlasRegion& R = *FaceRegions[f];
					ImVec2 uv0(static_cast<float>(R.X) / AtlasF, static_cast<float>(R.Y) / AtlasF);
					ImVec2 uv1(static_cast<float>(R.X + R.Size) / AtlasF, static_cast<float>(R.Y + R.Size) / AtlasF);

					if (f > 0 && f % 3 != 0) ImGui::SameLine();

					ImGui::BeginGroup();
					int32 FaceIdx = static_cast<int32>(R.FaceIdx);
					const char* FaceName = (FaceIdx >= 0 && FaceIdx < 6) ? FaceNames[FaceIdx] : "?";
					ImGui::Text("%s", FaceName);
					ImGui::Image(
						(ImTextureID)SR.Point.SRV,
						ImVec2(FacePreview, FacePreview),
						uv0, uv1,
						ImVec4(B, B, B, 1.0f), ImVec4(0.3f, 0.3f, 0.3f, 1.0f)
					);
					ImGui::EndGroup();
				}
			}
		}

		// ── 선택 없거나 못 찾으면 기존: 아틀라스 전체 표시 ──
		if (!bShowCropped && SR.Point.SRV)
		{
			float B = PointDepthBrightness;
			ImGui::Image(
				(ImTextureID)SR.Point.SRV,
				ImVec2(PreviewSize, PreviewSize),
				ImVec2(0, 0), ImVec2(1, 1),
				ImVec4(B, B, B, 1.0f), ImVec4(0.3f, 0.3f, 0.3f, 1.0f)
			);

			if (bShowPointRegions && pRegions)
				DrawRegionOverlay(*pRegions, PreviewSize, static_cast<float>(SR.Point.Resolution));
		}
	}

	ImGui::End();
}
