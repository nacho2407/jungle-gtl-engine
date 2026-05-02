#pragma once

#include "Component/PrimitiveComponent.h"

class FMeshBuffer;

class UCompletionOutlineComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UCompletionOutlineComponent, UPrimitiveComponent)

	UCompletionOutlineComponent() = default;
	~UCompletionOutlineComponent() override = default;

	void CreateRenderState() override;
	void DestroyRenderState() override;
	FMeshBuffer* GetMeshBuffer() const override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	float RemainingTime = 0.5f;
};
