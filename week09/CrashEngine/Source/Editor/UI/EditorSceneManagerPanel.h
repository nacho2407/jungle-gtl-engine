// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Editor/UI/EditorPanel.h"
#include "Core/CoreTypes.h"

// FEditorScenePanel는 에디터 UI 표시와 입력 처리를 담당합니다.
class FEditorScenePanel : public FEditorPanel
{
public:
    virtual void Initialize(UEditorEngine* InEditorEngine) override;
    virtual void Render(float DeltaTime) override;

private:
    void RefreshSceneFileList();
    void RenderActorOutliner();

    TArray<int32> ValidActorIndices;
    char SceneName[128] = "Default";

    TArray<FString> SceneFiles;
    int32 SelectedSceneIndex = -1;

    float NewSceneNotificationTimer = 0.f;
    float SceneSaveNotificationTimer = 0.f;
    float SceneLoadNotificationTimer = 0.f;
};
