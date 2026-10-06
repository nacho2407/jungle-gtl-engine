#pragma once

#include "Editor/UI/Panel/EditorPanelWidget.h"
#include "Profiling/Stats/Stats.h"

class FEditorStatWidget : public FEditorPanelWidget
{
public:
	void Render(const FEditorPanelContext& Context) override;

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
};
