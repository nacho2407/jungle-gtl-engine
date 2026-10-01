#pragma once

#include "Collision/OBB.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"

class UDecalComponent;

// FDecalSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
class FDecalSceneProxy : public FPrimitiveProxy
{
public:
    FDecalSceneProxy(UDecalComponent* InComponent);
    ~FDecalSceneProxy() override;

    void UpdateTransform() override;
    void UpdateMaterial() override;
    void UpdateMesh() override;
    const FOBB& GetDecalOBB() const { return CachedDecalOBB; }
    const FDecalCBData* GetDecalConstants() const;

private:
    UDecalComponent* GetDecalComponent() const;
    void             UpdateDecalConstants();

    FConstantBuffer* DecalCB;
    class UMaterial* DecalMaterial = nullptr;
    FOBB CachedDecalOBB;
};
