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

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char * PropertyName) override;
	
protected:
	// 각 섹션이 지니고 있는 메테리얼 정보를 오버라이드합니다.
	TArray<FMaterial*> OverrideMaterial;
};
