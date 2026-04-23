#pragma once
#include "PrimitiveComponent.h"

class UMaterialInterface;

class UMeshComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UMeshComponent, UPrimitiveComponent)

	virtual void Serialize(FArchive& Ar) override;

	virtual void SetMaterial(int32 SlotIndex, UMaterialInterface* InMaterial) override;
	virtual UMaterialInterface* GetMaterial(int32 SlotIndex) const override;

	const TArray<UMaterialInterface*>& GetOverrideMaterial() const;
	const std::pair<float, float> GetScroll() const { return ScrollUV; };

	virtual int32 GetNumMaterials() const override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char * PropertyName) override;
	
	virtual void TickComponent(float DeltaTime) override;

protected:
	TArray<UMaterialInterface*> Materials;
	std::pair<float, float> ScrollUV = { };
};
