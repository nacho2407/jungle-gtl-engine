#include "Editor/UI/EditorWorldSettingsWidget.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "Object/UClass.h"
#include "ImGui/imgui.h"

void EditorWorldSettingsWidget::Render()
{
	if (!bOpen) return;

	ImGui::SetNextWindowSize(ImVec2(360, 220), ImGuiCond_FirstUseEver);
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

	FWorldSettings& WS = World->GetWorldSettings();

	if (ImGui::CollapsingHeader("Game", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// GameMode 클래스 — UClass 레지스트리에서 AGameModeBase 파생만 필터링.
		// 첫 항목 "(Default)" = 빈 문자열 → ProjectSettings fallback.
		TArray<UClass*> GameModeClasses;
		GameModeClasses.push_back(nullptr); // sentinel for "(Default)"
		for (UClass* C : UClass::GetAllClasses())
		{
			if (C && C->IsA(AGameModeBase::StaticClass()))
				GameModeClasses.push_back(C);
		}

		int GMIdx = 0;
		for (int i = 1; i < static_cast<int>(GameModeClasses.size()); ++i)
		{
			if (WS.GameModeClassName == GameModeClasses[i]->GetName())
			{
				GMIdx = i;
				break;
			}
		}

		const char* GMPreview = (GMIdx == 0) ? "(Default)" : GameModeClasses[GMIdx]->GetName();
		if (ImGui::BeginCombo("GameMode Class", GMPreview))
		{
			for (int i = 0; i < static_cast<int>(GameModeClasses.size()); ++i)
			{
				const char* Label = (i == 0) ? "(Default)" : GameModeClasses[i]->GetName();
				bool bSelected = (i == GMIdx);
				if (ImGui::Selectable(Label, bSelected))
				{
					WS.GameModeClassName = (i == 0) ? FString() : FString(GameModeClasses[i]->GetName());
				}
				if (bSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::TextDisabled("Save scene + reload to apply.");
	}

	ImGui::End();
}
