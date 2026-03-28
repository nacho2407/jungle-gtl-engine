#include "StatWindow.h"
#include "Object/Object.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Memory/MemoryBase.h"

void FStatWindow::RefreshObjectList()
{
	ObjectEntries.clear();

	for (UObject* Obj : GUObjectArray)
	{
		if (Obj == nullptr)
		{
			continue;
		}

		FObjectEntry Entry;
		Entry.Name = Obj->GetName();
		Entry.ClassName = Obj->GetClass() ? Obj->GetClass()->GetName() : "Unknown";
		Entry.Size = Obj->ObjectSize;
		ObjectEntries.push_back(Entry);
	}

	bShowObjectList = true;
}

void FStatWindow::Render()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	bool bOpen = ImGui::Begin("Stats");
	ImGui::PopStyleVar();

	if (!bOpen)
	{
		ImGui::End();
		return;
	}

	ImGui::Text("FPS        : %.1f  (%.3f ms)", FPS, FrameTimeMs);
	ImGui::Text("Objects    : %u", ObjectCount);

	ImGui::Text("Current Heap Usage : %.2f KB", GetGMalloc()->MallocStats.CurrentAllocationBytes / 1024.0f);
	ImGui::Text("Current Heap Count : %d", GetGMalloc()->MallocStats.CurrentAllocationCount);
	
	ImGui::Separator();

	/** Legacy code */
	/*
	if (ImGui::Button("Refresh Object List"))
	{
		RefreshObjectList();
	}
	*/

	if (bShowObjectList && !ObjectEntries.empty())
	{
		ImGui::Text("Total: %d objects", static_cast<int>(ObjectEntries.size()));

		if (ImGui::BeginTable("ObjectTable", 3,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
			ImVec2(0, 300)))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Class");
			ImGui::TableSetupColumn("Size (bytes)");
			ImGui::TableHeadersRow();

			for (const FObjectEntry& Entry : ObjectEntries)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(Entry.Name.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(Entry.ClassName.c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%u", Entry.Size);
			}

			ImGui::EndTable();
		}
	}

	ImGui::End();
}
