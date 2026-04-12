#pragma once
#include "MeshComponent.h"
#include "Asset/StaticMesh.h"
#include "Render/Resource/Material.h"

class UStaticMeshComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)
	UStaticMeshComponent();
	
	virtual void PostDuplicate(UObject* Original) override;

	void SetStaticMesh(UStaticMesh* InStaticMesh);
	UStaticMesh* GetStaticMesh() const;
	bool HasValidMesh() const;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char * PropertyName) override;

	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_StaticMesh; }

	const FAABB& GetWorldAABB() const override;

	bool ConsumeRenderStateDirty();

private:
	void MarkBoundsDirty();
	void MarkRenderStateDirty();
	void EnsureBoundsUpdated() const;

private:
	UStaticMesh* StaticMeshAsset = nullptr;

	mutable bool bBoundsDirty = true;
	bool bRenderStateDirty = true;
};
