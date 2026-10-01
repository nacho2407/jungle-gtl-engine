#pragma once

#include "Render/Scene/Proxies/Primitive/BillboardSceneProxy.h"

class UCylindricalBillboardComponent;

// FCylindricalBillboardSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
class FCylindricalBillboardSceneProxy : public FBillboardSceneProxy
{
public:
    FCylindricalBillboardSceneProxy(UCylindricalBillboardComponent* InComponent);

    void UpdateMesh() override;
    void UpdatePerViewport(const FSceneView& SceneView) override;
};
