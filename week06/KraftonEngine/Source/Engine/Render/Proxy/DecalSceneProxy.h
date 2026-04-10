#pragma once
#include "PrimitiveSceneProxy.h"

class UDecalComponent;

class FDecalSceneProxy : public FPrimitiveSceneProxy
{
public:
	FDecalSceneProxy(UDecalComponent* InComponent);
	~FDecalSceneProxy() override = default;

	// FPrimitiveSceneProxy Interface
	void UpdateTransform() override;
	void UpdateMesh() override;
	void UpdateMaterial() override;

	void UpdatePerViewport(const FRenderBus& Bus) override;


private:
	void RebuildSectionDraw();

private:
	UDecalComponent* GetDecalComponent() const;
	void RebuildSectionDraws();
};

