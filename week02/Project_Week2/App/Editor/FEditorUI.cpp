#include "Editor/FEditorUI.h"

#include "imgui.h"
#include "FUObjectAllocator.h"
#include "UObject.h"

void FEditorUI::Render(const TArray<UObject*>& TestObjects)
{
    ImGui::Begin("Jungle Property Window");
    ImGui::Text("Hello Jungle World!");

    ImGui::Text("GTotalAllocationBytes: %d", FMemory::GetTotalAllocatedMemory());
    ImGui::Text("GTotalAllocationCount: %d", GUObjectArray.ElementalCount);
    ImGui::Text("ObjectCountInVector: %d", static_cast<int>(TestObjects.size()));

    if (!TestObjects.empty())
    {
        ImGui::Text("LastID: %d", TestObjects.back()->GetUUID());
    }

    ImGui::End();
}