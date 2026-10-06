#pragma once

#include "Render/Proxy/StaticMeshSceneProxy.h"
#include "Render/Resource/Buffer.h"

class UWheelMeshComponent;

class FWheelMeshSceneProxy : public FStaticMeshSceneProxy
{
public:
	explicit FWheelMeshSceneProxy(UWheelMeshComponent* InComponent);

	void ApplyDrawCommandOverrides(
		ID3D11Device* Device,
		ID3D11DeviceContext* Context,
		ERenderPass Pass,
		FDrawCommand& Command) const override;

private:
	UWheelMeshComponent* GetWheelMeshComponent() const;

	mutable FConstantBuffer WheelDeformationCB;
};
