// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Editor/Viewport/EditorViewportClient.h"

// FLevelEditorViewportClient는 카메라와 화면 출력에 필요한 상태를 다룹니다.
class FLevelEditorViewportClient : public FEditorViewportClient
{
public:
    FLevelEditorViewportClient() = default;
    ~FLevelEditorViewportClient() override = default;
};
