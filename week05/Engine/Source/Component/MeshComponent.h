#pragma once
#include "Component/NewPrimitiveComponent.h"

class FMaterial;

class ENGINE_API UMeshComponent : public UNewPrimitiveComponent
{
public:
	DECLARE_RTTI(UMeshComponent, UNewPrimitiveComponent)

	void SetMaterial(int32 Index, FMaterial* InMaterial);
	FMaterial* GetMaterial(int32 Index) const;
	int32 GetNumMaterials() const { return Materials.size(); }

	// virtual void Serialize(FArchive& Ar) override;

protected:
	TArray<FMaterial*> Materials;
};