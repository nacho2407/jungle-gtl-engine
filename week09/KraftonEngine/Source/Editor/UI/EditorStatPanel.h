// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Editor/UI/EditorPanel.h"
#include "Profiling/Stats.h"

// FEditorStatPanel는 에디터 UI 표시와 입력 처리를 담당합니다.
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
