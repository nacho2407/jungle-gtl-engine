#pragma once

#include "Render/Scene/Proxies/Primitive/BillboardSceneProxy.h"

class UTextRenderComponent;

// ============================================================
// FTextRenderSceneProxy — UTextRenderComponent 전용 프록시
// ============================================================
// Collector가 CachedText를 읽어 FFontGeometry로 배칭.
// PerObjectConstants는 SelectionMask 전용 아웃라인 행렬.
class FTextRenderSceneProxy : public FBillboardSceneProxy
{
public:
    FTextRenderSceneProxy(UTextRenderComponent* InComponent);

    void UpdateMesh() override;
    void UpdatePerViewport(const FSceneView& SceneView) override;

    // Collector가 FFontGeometry 배칭에 사용하는 캐싱된 텍스트 데이터
    FString CachedText;
    float CachedFontScale = 1.0f;
    FMatrix CachedBillboardMatrix;

private:
    UTextRenderComponent* GetTextRenderComponent() const;
};
