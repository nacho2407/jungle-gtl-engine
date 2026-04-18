#pragma once

#include "Editor/UI/EditorPanel.h"
#include "Profiling/Stats.h"

class FEditorStatPanel : public FEditorPanel
{
public:
	void Render(float DeltaTime) override;
	void RequestOpen();

private:
	void RenderStatTable(const char* TableID, const TArray<FStatEntry>& Source, int& OutSortColumn, bool& OutSortDescending, float TableHeight = 200.0f);

	int CPUSortColumn = 2;
	bool bCPUSortDescending = true;
	int GPUSortColumn = 2;
	bool bGPUSortDescending = true;
	bool bPaused = false;
	uint32 FrozenDrawCalls = 0;
	TArray<FStatEntry> FrozenCPUEntries;
	TArray<FStatEntry> FrozenGPUEntries;
	bool bRequestOpen = false;
};
