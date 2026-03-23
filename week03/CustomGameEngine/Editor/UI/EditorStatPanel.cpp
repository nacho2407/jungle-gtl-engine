#include "EditorStatPanel.h"

EditorStatPanel::EditorStatPanel()
{
}

EditorStatPanel::~EditorStatPanel()
{
}

void EditorStatPanel::Draw(ImGuiWindowLayout& layout)
{
	ImGui::Begin("Jungle Stats Panel");

	ImGui::Text("Allocation Count: %u", UEngineStatics::TotalAllocationCount);
	ImGui::Text("Allocation Bytes: %u", UEngineStatics::TotalAllocatedBytes);

	ImGui::End();
}
