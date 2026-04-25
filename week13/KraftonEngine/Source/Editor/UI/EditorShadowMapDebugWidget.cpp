#include "Editor/UI/EditorShadowMapDebugWidget.h"
#include "Editor/EditorEngine.h"
#include "Runtime/Engine.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/RenderPass/ShadowMapPass.h"
#include "ImGui/imgui.h"

void EditorShadowMapDebugWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!ImGui::Begin("Shadow Map Debug"))
	{
		ImGui::End();
		return;
	}

	FRenderer& Renderer = GEngine->GetRenderer();
	FShadowMapPass* ShadowPass = Renderer.GetPipeline().FindPass<FShadowMapPass>();

	if (!ShadowPass || !ShadowPass->HasValidShadow())
	{
		ImGui::TextDisabled("No valid shadow map");
		ImGui::End();
		return;
	}

	ID3D11ShaderResourceView* SRV = ShadowPass->GetShadowMapSRV();
	if (!SRV)
	{
		ImGui::TextDisabled("Shadow SRV is null");
		ImGui::End();
		return;
	}

	// Shadow map 프리뷰 — 윈도우 너비에 맞춰 정사각형
	float AvailWidth = ImGui::GetContentRegionAvail().x;
	float PreviewSize = (AvailWidth > 0.0f) ? AvailWidth : 256.0f;

	ImGui::Image(
		(ImTextureID)SRV,
		ImVec2(PreviewSize, PreviewSize),
		ImVec2(0, 0), ImVec2(1, 1),
		ImVec4(1, 1, 1, 1), ImVec4(0.3f, 0.3f, 0.3f, 1)
	);

	ImGui::End();
}
