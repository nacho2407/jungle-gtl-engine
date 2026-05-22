#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"

class UParticleSystemComponent;

class FParticleSystemSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FParticleSystemSceneProxy(UParticleSystemComponent* InComponent);

	void UpdateMesh() override;
	void UpdateMaterial() override;

private:
	UParticleSystemComponent* GetParticleSystemComponent() const;
};
