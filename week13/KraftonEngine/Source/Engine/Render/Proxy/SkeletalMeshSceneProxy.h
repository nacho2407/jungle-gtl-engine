#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"

class USkeletalMeshComponent;
class FDrawCommandBuffer;

class FSkeletalMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent);
	~FSkeletalMeshSceneProxy() override;

	void UpdateMaterial() override;
	void UpdateMesh() override;

	bool PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const override;
	
private:
	void RebuildSectionDraws();

private:
	USkeletalMeshComponent* GetSkeletalMeshComponent() const;

	mutable FDynamicVertexBuffer DynamicVertexBuffer;
	mutable TArray<FVertexPNCTT> SkinnedVertices;
};
