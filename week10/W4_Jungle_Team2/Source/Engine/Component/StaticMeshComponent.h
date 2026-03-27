#pragma once
#include "MeshComponent.h"
#include "Asset/StaticMesh.h"

class FStaticMesh;

class UStaticMeshComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)
	
	void SetStaticMesh(UStaticMesh* InStaticMesh);
	UStaticMesh* GetStaticMesh();
	bool HasValidMesh() const;
	
	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay & Ray, FHitResult & OutHitResult) override;
	
	const FAABB & GetWorldAABB() const override;
	
	void OnTransformChanged() override;
	
	bool ConsumRenderStateDirty();
	
private:
	void MarkBoundsDirty();
	void MarkRenderStateDirty();
	void EnsureBoundsUpdated() const;
	
private:
	UStaticMesh * StaticMeshAsset = nullptr;
	
	mutable FAABB WorldAABB;
	mutable bool bBoundsDirty = true;
	bool bRenderStateDirty = true;
};
