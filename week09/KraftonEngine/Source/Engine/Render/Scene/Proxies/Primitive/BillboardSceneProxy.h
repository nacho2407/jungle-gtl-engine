// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

class UBillboardComponent;

// FBillboardSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
class FBillboardSceneProxy : public FPrimitiveSceneProxy
{
public:
    FBillboardSceneProxy(UBillboardComponent* InComponent);

    void UpdateMesh() override;
    void UpdatePerViewport(const FSceneView& SceneView) override;

protected:
    UBillboardComponent* GetBillboardComponent() const;
};
