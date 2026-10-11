#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Resource/Buffer.h"

class UClothComponent;
struct FDrawCommandBuffer;

class FClothSceneProxy : public FPrimitiveSceneProxy
{
public:
	FClothSceneProxy(UClothComponent* InComponent);

public:
	void UpdateMaterial() override;
	void UpdateMesh() override;

	bool PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context,
		FDrawCommandBuffer& OutBuffer) const override;

private:
	UClothComponent* GetClothComponent() const;

private:
	mutable FDynamicVertexBuffer DynamicVertexBuffer;
	mutable FDynamicIndexBuffer DynamicIndexBuffer;
	mutable uint64 UploadedRevision = 0;
	mutable bool bBufferNeedsCreate = true;
};
