#pragma once

#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

class UTextRenderComponent;

/*
    FTextRenderSceneProxy는 UTextRenderComponent 전용 프록시입니다.
    실제 글자 쿼드는 font batch로 만들고, selection mask용 outline만 quad primitive 경로를 재사용합니다.
*/
class FTextRenderSceneProxy : public FPrimitiveSceneProxy
{
public:
    FTextRenderSceneProxy(UTextRenderComponent* InComponent);

    void UpdateMesh() override;
    void UpdatePerViewport(const FSceneView& SceneView) override;

    FString CachedText;
    float CachedFontScale = 1.0f;
    FVector CachedTextRight = FVector(0.0f, 1.0f, 0.0f);
    FVector CachedTextUp = FVector(0.0f, 0.0f, 1.0f);
    FMatrix CachedTextWorldMatrix;

private:
    UTextRenderComponent* GetTextRenderComponent() const;
};
