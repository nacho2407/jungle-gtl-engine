#pragma once

#include "Render/Scene/Proxies/Primitive/BillboardSceneProxy.h"

class UCylindricalBillboardComponent;

/*
    원통형 빌보드 컴포넌트를 렌더러용 데이터로 변환하는 프록시입니다.
    Y축 등 특정 축을 유지한 채 카메라 쪽으로만 회전하는 빌보드를 처리합니다.
*/
class FCylindricalBillboardSceneProxy : public FBillboardSceneProxy
{
public:
    FCylindricalBillboardSceneProxy(UCylindricalBillboardComponent* InComponent);

    void UpdateMesh() override;
    void UpdatePerViewport(const FSceneView& SceneView) override;
};
