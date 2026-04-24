// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Editor/UI/EditorPanel.h"
#include <string>

class FLevelEditorViewportClient;

// FEditorViewportPanel는 카메라와 화면 출력에 필요한 상태를 다룹니다.
class FEditorViewportPanel : public FEditorPanel
{
public:
    void Render(float DeltaTime) override;

    void SetViewportClient(FLevelEditorViewportClient* InClient) { ViewportClient = InClient; }
    void SetIndex(int32 InIndex);

private:
    FLevelEditorViewportClient* ViewportClient = nullptr;
    int32 Index = 0;
    std::string WindowName = "Viewport";
};
