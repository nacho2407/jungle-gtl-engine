#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Core/Stats.h"

class FEditorStatWidget : public FEditorWidget
{
public:
	void Render(float DeltaTime) override;

private:
	int SortColumn = 2;			// 기본 정렬: TotalTime
	bool bSortDescending = true;
	bool bPaused = false;		// true면 스냅샷 고정
	TArray<FStatEntry> FrozenEntries;	// Pause 시점의 스냅샷 복사본
};
