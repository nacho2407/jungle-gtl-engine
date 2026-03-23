#include "StatePanel.h"

#include "imgui.h"
#include "Editor/EditorContext.h"
#include "Engine/EngineStatics.h"

const wchar_t* FStatePanel::GetPanelID() const
{
	return L"StatePanel";
}

const wchar_t* FStatePanel::GetDisplayName() const
{
	return L"State Panel";
}

void FStatePanel::Draw()
{
	if (ImGui::Begin("State Panel", nullptr))
	{
		ImGui::Text("FPS                     : %.1f  (%.3f ms)", GetContext()->CurrentFPS, GetContext()->DeltaTime * 1000.f);
		ImGui::Text("TotalAllocationCount    : %u", UEngineStatics::TotalAllocationCount);
		ImGui::Text("Heap Usage              : %.2f KB", UEngineStatics::TotalAllocatedBytes / 1024.f);

	}
	ImGui::End();
}
