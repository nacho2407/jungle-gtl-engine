#pragma once

#include "Viewport/EditorViewport.h"
#include "Viewport/EditorViewportClient.h"

struct FEditorViewportPanel
{
    FEditorViewport*       Viewport;       // 렌더 타겟 텍스처 관리 (출력)
    FEditorViewportClient* ViewportClient; // 카메라 및 입력 로직 관리 (입력/제어)

    // 이 패널이 화면 어디에 그려지고 있는지 UI 좌표
    float PosX, PosY, Width, Height;
};
