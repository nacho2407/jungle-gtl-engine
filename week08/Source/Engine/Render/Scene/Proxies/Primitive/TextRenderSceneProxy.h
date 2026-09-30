#pragma once

#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"

class UTextRenderComponent;

// FTextRenderSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
class FTextRenderSceneProxy : public FPrimitiveProxy
{
public:
    FTextRenderSceneProxy(UTextRenderComponent* InComponent);

    void UpdateMesh() override;
    void UpdatePerViewport(const FSceneView& SceneView) override;

    FString CachedText;
    float   CachedFontScale = 1.0f;
    FVector CachedTextRight = FVector(0.0f, 1.0f, 0.0f);
    FVector CachedTextUp    = FVector(0.0f, 0.0f, 1.0f);
    FMatrix CachedTextWorldMatrix;

private:
    UTextRenderComponent* GetTextRenderComponent() const;
};
