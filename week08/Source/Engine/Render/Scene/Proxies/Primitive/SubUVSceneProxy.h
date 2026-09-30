#pragma once

#include "Render/Scene/Proxies/Primitive/BillboardSceneProxy.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"

class USubUVComponent;

// FSubUVSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
class FSubUVSceneProxy : public FBillboardSceneProxy
{
public:
    FSubUVSceneProxy(USubUVComponent* InComponent);
    ~FSubUVSceneProxy() override;

    void UpdateMesh() override;
    void UpdatePerViewport(const FSceneView& SceneView) override;

private:
    USubUVComponent* GetSubUVComponent() const;

    // 프록시별 UV region CB (공유 풀이 아닌 자체 소유)
    FConstantBuffer UVRegionCB;
};
