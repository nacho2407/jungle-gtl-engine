// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Editor/UI/EditorPanel.h"
#include "Math/Vector.h"

// FEditorControlPanel는 에디터 UI 표시와 입력 처리를 담당합니다.
class FEditorControlPanel : public FEditorPanel
{
public:
    virtual void Initialize(UEditorEngine* InEditorEngine) override;
    virtual void Render(float DeltaTime) override;

private:
    int32 SelectedPrimitiveType = 0;
    int32 NumberOfSpawnedActors = 1;
    FVector CurSpawnPoint = { 0.f, 0.f, 0.f };
};
