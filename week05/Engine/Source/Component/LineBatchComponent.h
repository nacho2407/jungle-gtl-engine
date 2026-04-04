#pragma once

#include "PrimitiveComponent.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Renderer/MeshData.h"

class FPrimitiveSceneProxy;

class ULineBatchComponent : public UPrimitiveComponent
{
	DECLARE_RTTI(ULineBatchComponent, UPrimitiveComponent)

public:
	void PostConstruct() override;
	void DrawLine(FVector InStart, FVector InEnd, FVector4 color);
	void DrawWireCube(FVector InCenter, FQuat InRotation, FVector InScale, FVector4 InColor);
	void DrawWireSphere(FVector InCenter, float InRadius, FVector4 InColor);
	void Clear();

	virtual FRenderMesh* GetRenderMesh() const override { return LineMesh.get(); }
	std::shared_ptr<FPrimitiveSceneProxy> CreateSceneProxy() const override;
	virtual FBoxSphereBounds GetLocalBounds() const override;

private:
	std::shared_ptr<FDynamicMesh> LineMesh;
};
