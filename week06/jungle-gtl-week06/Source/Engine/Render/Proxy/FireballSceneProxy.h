#pragma once
#include "Render/Proxy/PrimitiveSceneProxy.h"

class UFireballComponent;

class FFireballSceneProxy : public FPrimitiveSceneProxy
{
public:
	FFireballSceneProxy(UFireballComponent* InComponent);
	~FFireballSceneProxy() = default;

	void UpdatePerViewport(const FRenderBus& Bus) override;

	void UpdateMesh() override;

private:
	UFireballComponent* GetFireballComponent() const;
};

