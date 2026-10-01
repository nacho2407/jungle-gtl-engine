#pragma once

#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"

class UGizmoComponent;

// FGizmoSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
class FGizmoSceneProxy : public FPrimitiveProxy
{
public:
    FGizmoSceneProxy(UGizmoComponent* InComponent, bool bInner = false);

    void UpdateMesh() override;
    void UpdatePerViewport(const FSceneView& SceneView) override;

private:
    UGizmoComponent* GetGizmoComponent() const;
    bool             bIsInner = false;
};
