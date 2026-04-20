#pragma once

#include "Render/Scene/Proxies/Primitive/BillboardSceneProxy.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"

class USubUVComponent;

// ============================================================
// FSubUVSceneProxy — USubUVComponent 전용 프록시
// ============================================================
// Proxy path 렌더링.
// TexturedQuad + SubUV shader, 자체 CB로 UV region 전달.
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
