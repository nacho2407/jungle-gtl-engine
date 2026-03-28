#pragma once
#include "Component/UPrimitiveComponent.h"

class FMaterial;

class ENGINE_API UMeshComponent : public UUPrimitiveComponent
{
public:
	DECLARE_RTTI(UMeshComponent, UUPrimitiveComponent)

	void SetMaterial(int32 Index, FMaterial* InMaterial);
	FMaterial* GetMaterial(int32 Index) const;
	int32 GetNumMaterials() const { return Materials.size(); }

	virtual void Serialize(FArchive& Ar) override;

protected:
	TArray<FMaterial*> Materials;
};