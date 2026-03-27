#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Core/Stats.h"

class FEditorStatWidget : public FEditorWidget
{
public:
	void Render(float DeltaTime) override;

private:
	void RenderStatTable(const char* TableID, const TArray<FStatEntry>& Source, int& OutSortColumn, bool& OutSortDescending);

	int CPUSortColumn = 2;
	bool bCPUSortDescending = true;
	int GPUSortColumn = 2;
	bool bGPUSortDescending = true;
	bool bPaused = false;
	TArray<FStatEntry> FrozenCPUEntries;
	TArray<FStatEntry> FrozenGPUEntries;
};
