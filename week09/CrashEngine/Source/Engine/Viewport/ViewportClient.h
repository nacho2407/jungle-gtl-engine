// 뷰포트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

class FViewport;

// FViewportClient는 뷰포트의 렌더링과 입력 처리를 위임받는 인터페이스입니다.
class FViewportClient
{
public:
    FViewportClient() = default;
    virtual ~FViewportClient() = default;

    virtual void Draw(FViewport* Viewport, float DeltaTime) {}
    virtual bool InputKey(int32 Key, bool bPressed) { return false; }
    virtual bool InputAxis(float DeltaX, float DeltaY) { return false; }

    virtual FViewport* GetViewport() const { return nullptr; }
};
