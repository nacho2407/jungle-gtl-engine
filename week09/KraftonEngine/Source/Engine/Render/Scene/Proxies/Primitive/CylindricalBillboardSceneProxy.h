#pragma once

#include "Render/Scene/Proxies/Primitive/BillboardSceneProxy.h"

class UCylindricalBillboardComponent;

class FCylindricalBillboardSceneProxy : public FBillboardSceneProxy
{
public:
	FCylindricalBillboardSceneProxy(UCylindricalBillboardComponent* InComponent);

	void UpdateMesh() override;
	void UpdatePerViewport(const FFrameContext& Frame) override;
};
