#include "Editor/UI/Panel/EditorWorldSettingsWidget.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameMode/GameModeBase.h"
#include "Object/Reflection/UClass.h"
#include "ImGui/imgui.h"

void EditorWorldSettingsWidget::Render(const FEditorPanelContext& Context)
{
	(void)Context;

	if (!bOpen) return;

	ImGui::SetNextWindowSize(ImVec2(420, 360), ImGuiCond_FirstUseEver);
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

	if (ImGui::CollapsingHeader("Cloth Wind", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// scene 전체 cloth wind 값
		ImGui::Checkbox("Enabled", &WS.ClothWind.bEnabled);

		float Direction[3] = {
			WS.ClothWind.Direction.X,
			WS.ClothWind.Direction.Y,
			WS.ClothWind.Direction.Z
		};
		if (ImGui::DragFloat3("Direction", Direction, 0.1f))
		{
			WS.ClothWind.Direction = FVector(Direction[0], Direction[1], Direction[2]);
		}

		ImGui::DragFloat("Strength", &WS.ClothWind.Strength, 1.0f, 0.0f, 10000.0f);
		ImGui::DragFloat("Turbulence Strength", &WS.ClothWind.TurbulenceStrength, 1.0f, 0.0f, 10000.0f);
		ImGui::DragFloat("Turbulence Spatial Scale", &WS.ClothWind.TurbulenceSpatialScale, 1.0f, 0.001f, 10000.0f);
		ImGui::DragFloat("Turbulence Temporal Scale", &WS.ClothWind.TurbulenceTemporalScale, 0.01f, 0.0f, 100.0f);
		ImGui::DragInt("Turbulence Seed", &WS.ClothWind.TurbulenceSeed, 1.0f, 0, 1000000);
	}

	ImGui::End();
}
