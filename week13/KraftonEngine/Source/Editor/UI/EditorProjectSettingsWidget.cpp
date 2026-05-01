#include "Editor/UI/EditorProjectSettingsWidget.h"
#include "Core/ProjectSettings.h"
#include "Serialization/SceneSaveManager.h"
#include "ImGui/imgui.h"

void EditorProjectSettingsWidget::Render()
{
	if (!bOpen) return;

	ImGui::SetNextWindowSize(ImVec2(360, 200), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Project Settings", &bOpen))
	{
		ImGui::End();
		return;
	}

	FProjectSettings& PS = FProjectSettings::Get();

	if (ImGui::CollapsingHeader("Game", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// Scene 파일 목록을 콤보박스로 표시
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
				}
				if (bSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
	{
		static const char* kBackendLabels[] = { "Native", "PhysX" };
		int CurrentBackend = static_cast<int>(PS.Physics.Backend);
		if (ImGui::Combo("Backend", &CurrentBackend, kBackendLabels, 2))
		{
			PS.Physics.Backend = static_cast<EPhysicsBackend>(CurrentBackend);
		}
		ImGui::TextDisabled("Requires scene reload to take effect.");
	}

	if (ImGui::CollapsingHeader("Shadow", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Shadows", &PS.Shadow.bEnabled);
		if (PS.Shadow.bEnabled)
		{
			// Resolution 선택지 (power of 2)
			static const int kResOptions[] = { 64, 128, 256, 512, 1024, 2048, 4096, 8192 };
			static const char* kResLabels[] = { "64", "128", "256", "512", "1024", "2048", "4096", "8192" };
			constexpr int kNumRes = 8;

			auto ResCombo = [](const char* label, uint32& value) {
				int cur = 0;
				for (int i = 0; i < kNumRes; ++i)
					if (kResOptions[i] == static_cast<int>(value)) { cur = i; break; }
				if (ImGui::Combo(label, &cur, kResLabels, kNumRes))
					value = static_cast<uint32>(kResOptions[cur]);
			};

			ResCombo("CSM Resolution", PS.Shadow.CSMResolution);
			ResCombo("Spot Atlas Resolution", PS.Shadow.SpotAtlasResolution);
			ResCombo("Point Atlas Resolution", PS.Shadow.PointAtlasResolution);

			int spotPages = static_cast<int>(PS.Shadow.MaxSpotAtlasPages);
			if (ImGui::SliderInt("Max Spot Atlas Pages", &spotPages, 1, 16))
				PS.Shadow.MaxSpotAtlasPages = static_cast<uint32>(spotPages);

			int pointPages = static_cast<int>(PS.Shadow.MaxPointAtlasPages);
			if (ImGui::SliderInt("Max Point Atlas Pages", &pointPages, 1, 16))
				PS.Shadow.MaxPointAtlasPages = static_cast<uint32>(pointPages);
		}
	}

	ImGui::End();
}
