#pragma once
#include "PrimitiveComponent.h"

class FMaterial;

class UMeshComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UMeshComponent, UPrimitiveComponent)
	
	void SetMaterial(int32 SlotIndex, FMaterial* InMaterial);
	FMaterial* GetMaterial(int32 SlotIndex) const;

	const TArray<FMaterial*>& GetOverrideMaterial() const;
	const std::pair<float, float> GetScroll() const { return ScrollUV; };

	int32 GetMaterialCount() const;
	
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char * PropertyName) override;
	
	virtual void TickComponent(float DeltaTime) override;

protected:
	TArray<FMaterial*> OverrideMaterial;
	std::pair<float, float> ScrollUV = { };
};
