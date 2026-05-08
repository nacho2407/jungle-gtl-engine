#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"

class USkeletalMeshComponent;

class FSkeletalMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent);

	void UpdateMaterial() override;
	void UpdateMesh() override;
	
private:
	void RebuildSectionDraws();

private:
	USkeletalMeshComponent* GetSkeletalMeshComponent() const;
};
