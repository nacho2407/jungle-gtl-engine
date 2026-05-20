#include "EditorDragSource.h"
#include "imgui.h"

void EditorDragSource::Render(ImVec2 InSize)
{
	RenderSource(InSize);

	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload(DragID.c_str(), &DragSourceInfo, sizeof(DragSoruceInfo));
		RenderSource(InSize);
		ImGui::EndDragDropSource();
	}
}
