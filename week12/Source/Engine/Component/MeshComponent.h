#pragma once
#include "PrimitiveComponent.h"
#include "Render/Resource/Material.h"

UCLASS(Abstract)
class UMeshComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY(UMeshComponent, UPrimitiveComponent)

	virtual void Serialize(FArchive& Ar) override;

	virtual void SetMaterial(int32 SlotIndex, UMaterialInterface* InMaterial) override;
	virtual UMaterialInterface* GetMaterial(int32 SlotIndex) const override;

	const TArray<UMaterialInterface*>& GetOverrideMaterial() const;
	const TPair<float, float> GetScroll() const { return { ScrollU, ScrollV }; };

	virtual int32 GetNumMaterials() const override;
	void PostEditProperty(const char * PropertyName) override;
	
	virtual void TickComponent(float DeltaTime) override;

protected:
	UPROPERTY(DisplayName = "Materials", ReferenceType = Asset)
	TArray<UMaterialInterface*> Materials;

	UPROPERTY(DisplayName = "Scroll U", Min = -1.0f, Max = 1.0f, Speed = 0.01f)
	float ScrollU = 0.0f;

	UPROPERTY(DisplayName = "Scroll V", Min = -1.0f, Max = 1.0f, Speed = 0.01f)
	float ScrollV = 0.0f;
};
