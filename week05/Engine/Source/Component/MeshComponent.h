#pragma once
#include "Component/PrimitiveComponent.h"

class FMaterial;

class ENGINE_API UMeshComponent : public UPrimitiveComponent
{
public:
	DECLARE_RTTI(UMeshComponent, UPrimitiveComponent)

	void SetMaterial(int32 Index, const std::shared_ptr<FMaterial>& InMaterial);
	std::shared_ptr<FMaterial> GetMaterial(int32 Index) const;
	int32 GetNumMaterials() const { return static_cast<int32>(Materials.size()); }
	void DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const override;

	void Serialize(FArchive& Ar) override;

protected:
	void DuplicateMaterialsTo(UMeshComponent* DuplicatedComponent) const;
	TArray<std::shared_ptr<FMaterial>> Materials;
};
