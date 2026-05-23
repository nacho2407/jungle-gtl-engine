#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"

class UParticleSystemComponent;

class FParticleSystemSceneProxy : public FPrimitiveSceneProxy
{
public:
    FParticleSystemSceneProxy(UParticleSystemComponent* InComponent);
    ~FParticleSystemSceneProxy() override = default;

    void UpdateTransform() override;
    void UpdateMesh() override;
    void UpdateMaterial() override;

private:
    UParticleSystemComponent* GetParticleSystemComponent() const;
};
