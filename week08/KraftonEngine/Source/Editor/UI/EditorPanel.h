// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

class UEditorEngine;

// FEditorPanel는 에디터 UI 표시와 입력 처리를 담당합니다.
class FEditorPanel
{
public:
    virtual ~FEditorPanel() = default;

    virtual void Initialize(UEditorEngine* InEditorEngine);
    virtual void Render(float DeltaTime) = 0;

protected:
    UEditorEngine* EditorEngine = nullptr;
};
