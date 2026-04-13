#pragma once
#include "PrimitiveComponent.h"

class UMaterialInterface;

class UMeshComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UMeshComponent, UPrimitiveComponent)
	
	void SetMaterial(int32 SlotIndex, UMaterialInterface* InMaterial);
	UMaterialInterface* GetMaterial(int32 SlotIndex) const;

	const TArray<UMaterialInterface*>& GetOverrideMaterial() const;
	const std::pair<float, float> GetScroll() const { return ScrollUV; };

	int32 GetMaterialCount() const;
	
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char * PropertyName) override;
	
	virtual void TickComponent(float DeltaTime) override;

protected:
	TArray<UMaterialInterface*> OverrideMaterial;
	std::pair<float, float> ScrollUV = { };
};
