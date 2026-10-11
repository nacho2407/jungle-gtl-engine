#include "Editor/UI/Panel/EditorProjectSettingsWidget.h"
#include "Core/ProjectSettings.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Pawn/Pawn.h"
#include "Object/Reflection/UClass.h"
#include "Serialization/SceneSaveManager.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cstdio>

namespace
{
bool InputFString(const char* Label, FString& Value, size_t BufferSize = 256)
{
	char Buffer[512] = {};
	const size_t SafeBufferSize = (std::min)(BufferSize, sizeof(Buffer));
	std::snprintf(Buffer, SafeBufferSize, "%s", Value.c_str());
	if (ImGui::InputText(Label, Buffer, SafeBufferSize))
	{
		Value = Buffer;
		return true;
	}
	return false;
}

bool ClassNameCombo(const char* Label, UClass* RequiredBaseClass, FString& InOutClassName, const char* FallbackLabel)
{
	TArray<UClass*> Classes;
	for (UClass* Class : UClass::GetAllClasses())
	{
		if (Class && (!RequiredBaseClass || Class->IsA(RequiredBaseClass)))
		{
			Classes.push_back(Class);
		}
	}

	const char* Preview = InOutClassName.empty() ? FallbackLabel : InOutClassName.c_str();
	bool bChanged = false;
	if (ImGui::BeginCombo(Label, Preview))
	{
		for (UClass* Class : Classes)
		{
			const char* ClassName = Class ? Class->GetName() : "";
			const bool bSelected = InOutClassName == ClassName;
			if (ImGui::Selectable(ClassName, bSelected))
			{
				InOutClassName = ClassName;
				bChanged = true;
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	return bChanged;
}
}

void EditorProjectSettingsWidget::Render()
{
	if (!bOpen) return;

	ImGui::SetNextWindowSize(ImVec2(520, 440), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Project Settings", &bOpen))
	{
		ImGui::End();
		return;
	}

	FProjectSettings& PS = FProjectSettings::Get();
	bool bSettingsChanged = false;

	if (ImGui::CollapsingHeader("Game", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// 시작 scene 파일 목록 표시
		TArray<FString> SceneFiles = FSceneSaveManager::GetSceneFileList();

		int CurrentIdx = -1;
		for (int i = 0; i < static_cast<int>(SceneFiles.size()); ++i)
		{
			if (SceneFiles[i] == PS.Game.StartLevelName)
			{
				CurrentIdx = i;
				break;
			}
		}

		const char* Preview = CurrentIdx >= 0 ? SceneFiles[CurrentIdx].c_str() : "(None)";
		if (ImGui::BeginCombo("Start Level", Preview))
		{
			for (int i = 0; i < static_cast<int>(SceneFiles.size()); ++i)
			{
				bool bSelected = (i == CurrentIdx);
				if (ImGui::Selectable(SceneFiles[i].c_str(), bSelected))
				{
					PS.Game.StartLevelName = SceneFiles[i];
					bSettingsChanged = true;
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Gameplay Preset");

		auto& Preset = PS.Game.GameplayPreset;
		bSettingsChanged |= InputFString("Director Module", Preset.DirectorModule);
		bSettingsChanged |= ClassNameCombo(
			"Player Controller Class",
			APlayerController::StaticClass(),
			Preset.PlayerControllerClassName,
			"ALuaPlayerController");
		bSettingsChanged |= ClassNameCombo(
			"Default Pawn Class",
			APawn::StaticClass(),
			Preset.DefaultPawnClassName,
			"ALuaCharacter");
		bSettingsChanged |= InputFString("Default Pawn Script", Preset.DefaultPawnScript);
		bSettingsChanged |= InputFString("Default Pawn Mesh", Preset.DefaultPawnMeshPath, 512);
		bSettingsChanged |= InputFString("Default Player Start Tag", Preset.DefaultPlayerStartTag);
		bSettingsChanged |= ImGui::Checkbox("Use Placed Auto Possess Pawn", &Preset.bUsePlacedAutoPossessPawn);
		bSettingsChanged |= ImGui::Checkbox("Spawn Default Pawn If Missing", &Preset.bSpawnDefaultPawnIfMissing);
		ImGui::TextDisabled("Gameplay preset is applied on the next PIE or standalone scene start.");
	}

	if (ImGui::CollapsingHeader("Shadow", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Checkbox("Shadows", &PS.Shadow.bEnabled))
		{
			bSettingsChanged = true;
		}
		if (PS.Shadow.bEnabled)
		{
			// Shadow map 해상도 선택지
			static const int kResOptions[] = { 64, 128, 256, 512, 1024, 2048, 4096, 8192 };
			static const char* kResLabels[] = { "64", "128", "256", "512", "1024", "2048", "4096", "8192" };
			constexpr int kNumRes = 8;

			auto ResCombo = [](const char* Label, uint32& Value)
			{
				int Current = 0;
				for (int i = 0; i < kNumRes; ++i)
				{
					if (kResOptions[i] == static_cast<int>(Value))
					{
						Current = i;
						break;
					}
				}
				if (ImGui::Combo(Label, &Current, kResLabels, kNumRes))
				{
					Value = static_cast<uint32>(kResOptions[Current]);
					return true;
				}
				return false;
			};

			bSettingsChanged |= ResCombo("CSM Resolution", PS.Shadow.CSMResolution);

			if (ImGui::InputFloat("Directional Shadow Distance", &PS.Shadow.DirectionalShadowDistance, 10.0f, 100.0f, "%.3f"))
			{
				PS.Shadow.DirectionalShadowDistance = (std::max)(0.0f, PS.Shadow.DirectionalShadowDistance);
				bSettingsChanged = true;
			}
			if (ImGui::Checkbox("Directional Shadow Fade Out", &PS.Shadow.bDirectionalShadowFadeOut))
			{
				bSettingsChanged = true;
			}

			bSettingsChanged |= ResCombo("Spot Atlas Resolution", PS.Shadow.SpotAtlasResolution);
			bSettingsChanged |= ResCombo("Point Atlas Resolution", PS.Shadow.PointAtlasResolution);

			int SpotPages = static_cast<int>(PS.Shadow.MaxSpotAtlasPages);
			if (ImGui::SliderInt("Max Spot Atlas Pages", &SpotPages, 1, 16))
			{
				PS.Shadow.MaxSpotAtlasPages = static_cast<uint32>(SpotPages);
				bSettingsChanged = true;
			}

			int PointPages = static_cast<int>(PS.Shadow.MaxPointAtlasPages);
			if (ImGui::SliderInt("Max Point Atlas Pages", &PointPages, 1, 16))
			{
				PS.Shadow.MaxPointAtlasPages = static_cast<uint32>(PointPages);
				bSettingsChanged = true;
			}
		}
	}

	if (bSettingsChanged)
	{
		PS.SaveToFile(FProjectSettings::GetDefaultPath());
	}

	ImGui::End();
}
