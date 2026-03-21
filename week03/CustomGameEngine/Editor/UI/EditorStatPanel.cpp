#include "EditorStatPanel.h"

EditorStatPanel::EditorStatPanel()
{
}

EditorStatPanel::~EditorStatPanel()
{
}

void EditorStatPanel::Draw()
{
	ImGui::SetNextWindowPos(ImVec2(10, 500), ImGuiCond_FirstUseEver);
	ImGui::Begin("Jungle Stats Panel");

	ImGui::Text("Allocation Count: %u", UEngineStatics::TotalAllocationCount);
	ImGui::Text("Allocation Bytes: %u", UEngineStatics::TotalAllocatedBytes);

	ImGui::End();
}