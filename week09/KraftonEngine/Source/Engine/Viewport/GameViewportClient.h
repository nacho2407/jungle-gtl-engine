// 뷰포트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Object/Object.h"
#include "Viewport/ViewportClient.h"

class FViewport;

// UGameViewportClient는 게임 월드와 실제 뷰포트 출력을 연결합니다.
class UGameViewportClient : public UObject, public FViewportClient
{
public:
    DECLARE_CLASS(UGameViewportClient, UObject)

    UGameViewportClient() = default;
    ~UGameViewportClient() override = default;

    // FViewportClient 인터페이스입니다.
    void Draw(FViewport* Viewport, float DeltaTime) override {}
    bool InputKey(int32 Key, bool bPressed) override { return false; }

    // 렌더링 대상 뷰포트를 설정합니다.
    void SetViewport(FViewport* InViewport) { Viewport = InViewport; }
    FViewport* GetViewport() const override { return Viewport; }

private:
    FViewport* Viewport = nullptr;
};
