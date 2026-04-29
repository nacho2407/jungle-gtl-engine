#include "Editor/UI/EditorProjectSettingsWidget.h"
#include "Core/ProjectSettings.h"
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
