#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Settings/ProjectSettings.h"
#include "Engine/Core/Paths.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "Core/Reflection/ReflectionRegistry.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cwctype>
#include <cstring>
#include <filesystem>

namespace
{
TArray<UClass*> GetRegisteredTypesAssignableTo(UClass* BaseType)
{
	TArray<UClass*> Types;
	if (!BaseType)
	{
		return Types;
	}

	FReflectionRegistry::Get().GetClassesDerivedFrom(BaseType, Types);

	std::sort(
		Types.begin(),
		Types.end(),
		[](const UClass* A, const UClass* B)
		{
			const char* AName = A ? A->GetName() : "";
			const char* BName = B ? B->GetName() : "";
			return std::strcmp(AName, BName) < 0;
		});
	return Types;
}

bool DrawClassCombo(const char* Label, char* Buffer, size_t BufferSize, UClass* BaseType)
{
	bool bChanged = false;
	TArray<UClass*> Types = GetRegisteredTypesAssignableTo(BaseType);
	const char* CurrentLabel = Buffer && Buffer[0] != '\0' ? Buffer : "None";

	ImGui::SetNextItemWidth(260.0f);
	if (ImGui::BeginCombo(Label, CurrentLabel))
	{
		for (UClass* Type : Types)
		{
			if (!Type || !Type->GetName())
			{
				continue;
			}

			const bool bSelected = Buffer && std::strcmp(Buffer, Type->GetName()) == 0;
			if (ImGui::Selectable(Type->GetName(), bSelected))
			{
				strncpy_s(Buffer, BufferSize, Type->GetName(), _TRUNCATE);
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

TArray<FString> CollectPrefabAssetPaths()
{
	TArray<FString> PrefabPaths;
	const std::filesystem::path PrefabRoot = std::filesystem::path(FPaths::RootDir()) / L"Asset" / L"Prefab";
	std::error_code Ec;
	if (!std::filesystem::exists(PrefabRoot, Ec))
	{
		return PrefabPaths;
	}

	for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(PrefabRoot, Ec))
	{
		if (Ec || !Entry.is_regular_file())
		{
			continue;
		}

		std::wstring Extension = Entry.path().extension().generic_wstring();
		std::transform(
			Extension.begin(),
			Extension.end(),
			Extension.begin(),
			[](wchar_t Ch)
			{
				return static_cast<wchar_t>(std::towlower(Ch));
			});
		if (Extension != L".prefab")
		{
			continue;
		}

		PrefabPaths.push_back(FPaths::ToRelativeString(Entry.path().wstring()));
	}

	std::sort(PrefabPaths.begin(), PrefabPaths.end());
	return PrefabPaths;
}

bool DrawPrefabCombo(const char* Label, char* Buffer, size_t BufferSize)
{
	bool bChanged = false;
	const TArray<FString> PrefabPaths = CollectPrefabAssetPaths();
	const char* CurrentLabel = Buffer && Buffer[0] != '\0' ? Buffer : "None";

	ImGui::SetNextItemWidth(360.0f);
	if (ImGui::BeginCombo(Label, CurrentLabel))
	{
		const bool bNoneSelected = !Buffer || Buffer[0] == '\0';
		if (ImGui::Selectable("None", bNoneSelected))
		{
			if (Buffer && BufferSize > 0)
			{
				Buffer[0] = '\0';
			}
			bChanged = true;
		}
		if (bNoneSelected)
		{
			ImGui::SetItemDefaultFocus();
		}

		ImGui::Separator();
		if (PrefabPaths.empty())
		{
			ImGui::BeginDisabled();
			ImGui::Selectable("Asset/Prefab contains no .prefab files", false);
			ImGui::EndDisabled();
		}
		for (const FString& PrefabPath : PrefabPaths)
		{
			const bool bSelected = Buffer && std::strcmp(Buffer, PrefabPath.c_str()) == 0;
			if (ImGui::Selectable(PrefabPath.c_str(), bSelected))
			{
				strncpy_s(Buffer, BufferSize, PrefabPath.c_str(), _TRUNCATE);
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
} // namespace

void FEditorMainPanel::LoadGameModeSettingsPanelBuffers()
{
	FProjectSettings& ProjectSettings = FProjectSettings::Get();
	ProjectSettings.LoadFromFile(FProjectSettings::GetDefaultSettingsPath());

	FGameBuildSettings& Settings = ProjectSettings.BuildSettings;
	if (Settings.GameModeClass.empty())
	{
		Settings.GameModeClass = "AGameModeBase";
	}
	if (Settings.PlayerControllerClass.empty())
	{
		Settings.PlayerControllerClass = "APlayerController";
	}
	if (Settings.DefaultPawnClass.empty())
	{
		Settings.DefaultPawnClass = "ADefaultPawn";
	}

	strncpy_s(GameModeSettingsState.GameModeClassBuffer, Settings.GameModeClass.c_str(), _TRUNCATE);
	strncpy_s(GameModeSettingsState.PlayerControllerClassBuffer, Settings.PlayerControllerClass.c_str(), _TRUNCATE);
	strncpy_s(GameModeSettingsState.DefaultPawnClassBuffer, Settings.DefaultPawnClass.c_str(), _TRUNCATE);
	strncpy_s(GameModeSettingsState.DefaultPawnPrefabPathBuffer, Settings.DefaultPawnPrefabPath.c_str(), _TRUNCATE);

	LoadWorldGameModeSettingsPanelBuffers();
	GameModeSettingsState.bLoaded = true;
}

void FEditorMainPanel::LoadWorldGameModeSettingsPanelBuffers()
{
	UWorld* World = EditorEngine ? EditorEngine->GetFocusedWorld() : nullptr;
	GameModeSettingsState.CachedWorld = World;
	const FWorldGameModeSettings SceneSettings = World ? World->GetGameModeSettings() : FWorldGameModeSettings{};
	GameModeSettingsState.bSceneOverrideGameMode = SceneSettings.bOverrideGameMode;
	strncpy_s(GameModeSettingsState.SceneGameModeClassBuffer, SceneSettings.GameModeClass.c_str(), _TRUNCATE);
	strncpy_s(GameModeSettingsState.ScenePlayerControllerClassBuffer, SceneSettings.PlayerControllerClass.c_str(), _TRUNCATE);
	strncpy_s(GameModeSettingsState.SceneDefaultPawnClassBuffer, SceneSettings.DefaultPawnClass.c_str(), _TRUNCATE);
	strncpy_s(GameModeSettingsState.SceneDefaultPawnPrefabPathBuffer, SceneSettings.DefaultPawnPrefabPath.c_str(), _TRUNCATE);
}

void FEditorMainPanel::SaveProjectGameModeSettingsPanelBuffers()
{
	FProjectSettings& ProjectSettings = FProjectSettings::Get();
	ProjectSettings.LoadFromFile(FProjectSettings::GetDefaultSettingsPath());

	FGameBuildSettings& Settings = ProjectSettings.BuildSettings;
	Settings.GameModeClass = GameModeSettingsState.GameModeClassBuffer[0] != '\0'
		? FString(GameModeSettingsState.GameModeClassBuffer)
		: FString("AGameModeBase");
	Settings.PlayerControllerClass = GameModeSettingsState.PlayerControllerClassBuffer[0] != '\0'
		? FString(GameModeSettingsState.PlayerControllerClassBuffer)
		: FString("APlayerController");
	Settings.DefaultPawnClass = GameModeSettingsState.DefaultPawnClassBuffer[0] != '\0'
		? FString(GameModeSettingsState.DefaultPawnClassBuffer)
		: FString("ADefaultPawn");
	Settings.DefaultPawnPrefabPath = GameModeSettingsState.DefaultPawnPrefabPathBuffer;
	ProjectSettings.SaveToFile(FProjectSettings::GetDefaultSettingsPath());
}

void FEditorMainPanel::SaveWorldGameModeSettingsPanelBuffers()
{
	if (UWorld* World = EditorEngine ? EditorEngine->GetFocusedWorld() : nullptr)
	{
		FWorldGameModeSettings SceneSettings;
		SceneSettings.bOverrideGameMode = GameModeSettingsState.bSceneOverrideGameMode;
		SceneSettings.GameModeClass = GameModeSettingsState.SceneGameModeClassBuffer[0] != '\0'
			? FString(GameModeSettingsState.SceneGameModeClassBuffer)
			: FString("AGameModeBase");
		SceneSettings.PlayerControllerClass = GameModeSettingsState.ScenePlayerControllerClassBuffer[0] != '\0'
			? FString(GameModeSettingsState.ScenePlayerControllerClassBuffer)
			: FString("APlayerController");
		SceneSettings.DefaultPawnClass = GameModeSettingsState.SceneDefaultPawnClassBuffer[0] != '\0'
			? FString(GameModeSettingsState.SceneDefaultPawnClassBuffer)
			: FString("ADefaultPawn");
		SceneSettings.DefaultPawnPrefabPath = GameModeSettingsState.SceneDefaultPawnPrefabPathBuffer;
		World->SetGameModeSettings(SceneSettings);
		EditorEngine->GetSceneService().MarkDirty();
	}
}

void FEditorMainPanel::RenderProjectSettingsPanel()
{
	if (!GameModeSettingsState.bLoaded)
	{
		LoadGameModeSettingsPanelBuffers();
	}

	ImGui::SetNextWindowSize(ImVec2(500.0f, 280.0f), ImGuiCond_FirstUseEver);
	bool bOpen = PanelVisibility.bShowProjectSettings;
	if (!ImGui::Begin("Project Settings", &bOpen))
	{
		PanelVisibility.bShowProjectSettings = bOpen;
		ImGui::End();
		return;
	}
	PanelVisibility.bShowProjectSettings = bOpen;

	ImGui::TextUnformatted("Game Mode Defaults");
	ImGui::Separator();

	DrawClassCombo(
		"Game Mode Class",
		GameModeSettingsState.GameModeClassBuffer,
		IM_ARRAYSIZE(GameModeSettingsState.GameModeClassBuffer),
		AGameModeBase::StaticClass());

	DrawClassCombo(
		"Player Controller Class",
		GameModeSettingsState.PlayerControllerClassBuffer,
		IM_ARRAYSIZE(GameModeSettingsState.PlayerControllerClassBuffer),
		APlayerController::StaticClass());

	DrawClassCombo(
		"Default Pawn Class",
		GameModeSettingsState.DefaultPawnClassBuffer,
		IM_ARRAYSIZE(GameModeSettingsState.DefaultPawnClassBuffer),
		APawn::StaticClass());

	DrawPrefabCombo(
		"Default Pawn Prefab",
		GameModeSettingsState.DefaultPawnPrefabPathBuffer,
		IM_ARRAYSIZE(GameModeSettingsState.DefaultPawnPrefabPathBuffer));
	ImGui::TextDisabled("Used when the current world does not override Game Mode.");
	ImGui::TextDisabled("If a pawn prefab is set, it can override Default Pawn Class at spawn time.");

	ImGui::Separator();
	if (ImGui::Button("Save Project", ImVec2(112.0f, 0.0f)))
	{
		SaveProjectGameModeSettingsPanelBuffers();
	}
	ImGui::SameLine();
	if (ImGui::Button("Refresh", ImVec2(84.0f, 0.0f)))
	{
		LoadGameModeSettingsPanelBuffers();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset", ImVec2(84.0f, 0.0f)))
	{
		strncpy_s(GameModeSettingsState.GameModeClassBuffer, "AGameModeBase", _TRUNCATE);
		strncpy_s(GameModeSettingsState.PlayerControllerClassBuffer, "APlayerController", _TRUNCATE);
		strncpy_s(GameModeSettingsState.DefaultPawnClassBuffer, "ADefaultPawn", _TRUNCATE);
		strncpy_s(GameModeSettingsState.DefaultPawnPrefabPathBuffer, "", _TRUNCATE);
	}

	ImGui::End();
}

void FEditorMainPanel::RenderWorldSettingsPanel()
{
	UWorld* CurrentWorld = EditorEngine ? EditorEngine->GetFocusedWorld() : nullptr;
	if (!GameModeSettingsState.bLoaded)
	{
		LoadGameModeSettingsPanelBuffers();
	}
	else if (GameModeSettingsState.CachedWorld != CurrentWorld)
	{
		LoadWorldGameModeSettingsPanelBuffers();
	}

	ImGui::SetNextWindowSize(ImVec2(500.0f, 280.0f), ImGuiCond_FirstUseEver);
	bool bOpen = PanelVisibility.bShowWorldSettings;
	if (!ImGui::Begin("World Settings", &bOpen))
	{
		PanelVisibility.bShowWorldSettings = bOpen;
		ImGui::End();
		return;
	}
	PanelVisibility.bShowWorldSettings = bOpen;

	ImGui::TextUnformatted("Current World Game Mode Override");
	ImGui::Separator();

	ImGui::Checkbox("Override Game Mode for Current World", &GameModeSettingsState.bSceneOverrideGameMode);
	ImGui::BeginDisabled(!GameModeSettingsState.bSceneOverrideGameMode);

	DrawClassCombo(
		"World Game Mode",
		GameModeSettingsState.SceneGameModeClassBuffer,
		IM_ARRAYSIZE(GameModeSettingsState.SceneGameModeClassBuffer),
		AGameModeBase::StaticClass());

	DrawClassCombo(
		"World Player Controller",
		GameModeSettingsState.ScenePlayerControllerClassBuffer,
		IM_ARRAYSIZE(GameModeSettingsState.ScenePlayerControllerClassBuffer),
		APlayerController::StaticClass());

	DrawClassCombo(
		"World Default Pawn",
		GameModeSettingsState.SceneDefaultPawnClassBuffer,
		IM_ARRAYSIZE(GameModeSettingsState.SceneDefaultPawnClassBuffer),
		APawn::StaticClass());

	DrawPrefabCombo(
		"World Pawn Prefab",
		GameModeSettingsState.SceneDefaultPawnPrefabPathBuffer,
		IM_ARRAYSIZE(GameModeSettingsState.SceneDefaultPawnPrefabPathBuffer));
	ImGui::EndDisabled();
	ImGui::TextDisabled("World settings are saved into the .scene file.");
	ImGui::TextDisabled("Pawn prefab root must derive from APawn.");

	ImGui::Separator();
	if (ImGui::Button("Save World", ImVec2(112.0f, 0.0f)))
	{
		SaveWorldGameModeSettingsPanelBuffers();
	}
	ImGui::SameLine();
	if (ImGui::Button("Refresh", ImVec2(84.0f, 0.0f)))
	{
		LoadGameModeSettingsPanelBuffers();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset", ImVec2(84.0f, 0.0f)))
	{
		GameModeSettingsState.bSceneOverrideGameMode = false;
		strncpy_s(GameModeSettingsState.SceneGameModeClassBuffer, "AGameModeBase", _TRUNCATE);
		strncpy_s(GameModeSettingsState.ScenePlayerControllerClassBuffer, "APlayerController", _TRUNCATE);
		strncpy_s(GameModeSettingsState.SceneDefaultPawnClassBuffer, "ADefaultPawn", _TRUNCATE);
		strncpy_s(GameModeSettingsState.SceneDefaultPawnPrefabPathBuffer, "", _TRUNCATE);
	}

	ImGui::End();
}
