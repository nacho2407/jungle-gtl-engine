#pragma once

#include "BillboardSceneProxy.h"

class UCylindricalBillboardComponent;

class FCylindricalBillboardSceneProxy : public FBillboardSceneProxy
{
public:
	FCylindricalBillboardSceneProxy(UCylindricalBillboardComponent* InComponent);

	void UpdateTransform() override;
	void UpdateMesh() override;
	void UpdatePerViewport(const FFrameContext& Frame) override;

protected:
	FVector CachedWorldAxis;
};
