#pragma once
#include "PrimitiveComponent.h"

class FMaterial;

class UMeshComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UMeshComponent, UPrimitiveComponent)
	
	// UpdateWorldAABB 등의 함수를 오버라이드하지 않았기 때문에 UMeshComponent도 추상 클래스가 됩니다.
	// 추후에 MeshComponent를 사용할 일이 있다면 Duplicate의 주석을 해제하고 수정하시면 됩니다.
	virtual UMeshComponent* Duplicate() override = 0;
	virtual UMeshComponent* DuplicateSubObjects() override { return this; }

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
