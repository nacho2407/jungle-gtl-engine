#pragma once
#include "PrimitiveComponent.h"

class UMaterial;

class UMeshComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UMeshComponent, UPrimitiveComponent)
	
	
	void SetMaterial(int32 SlotIndex, UMaterial * InMaterial);
	UMaterial * GetMaterial(int32 SlotIndex) const;
	int32 GetMaterialCount() const;
	
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char * PropertyName) override;
	
protected:
	// TODO : 머티리얼 교체지원 FMaterial -> UMaterial
	TArray<UMaterial*> OverrideMaterial;
};
