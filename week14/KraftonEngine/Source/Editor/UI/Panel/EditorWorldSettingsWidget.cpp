#include "Editor/UI/Panel/EditorWorldSettingsWidget.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/World.h"
#include "GameFramework/WorldSettings.h"
#include "ImGui/imgui.h"

void EditorWorldSettingsWidget::Render()
{
	if (!bOpen) return;

	ImGui::SetNextWindowSize(ImVec2(360, 160), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("World Settings", &bOpen))
	{
		ImGui::End();
		return;
	}

	UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;
	if (!World)
	{
		ImGui::TextDisabled("No active world.");
		ImGui::End();
		return;
	}

	// 현재는 scene별 gameplay override를 저장하지 않습니다.
	(void)World->GetWorldSettings();

	if (ImGui::CollapsingHeader("Game", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextDisabled("Scene-specific GameMode override has been removed.");
		ImGui::TextDisabled("Gameplay startup uses Project Settings > Gameplay Preset.");
	}

	ImGui::End();
}
