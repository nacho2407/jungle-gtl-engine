#pragma once

#include "NewPrimitiveComponent.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "PrimitiveLineBatch.h"
#include "Renderer/MeshData.h"

class ULineBatchComponent : public UNewPrimitiveComponent
{
	DECLARE_RTTI(ULineBatchComponent, UNewPrimitiveComponent)

public:
	void PostConstruct() override;
	void DrawLine(FVector InStart, FVector InEnd, FVector4 color);
	void DrawWireCube(FVector InCenter, FQuat InRotation, FVector InScale, FVector4 InColor);
	void DrawWireSphere(FVector InCenter, float InRadius, FVector4 InColor);
	void Clear();

	virtual FRenderMesh* GetRenderMesh() const override { return LineMesh.get(); }
	virtual FBoxSphereBounds GetLocalBounds() const override;

private:
	std::shared_ptr<FDynamicMesh> LineMesh;
};
