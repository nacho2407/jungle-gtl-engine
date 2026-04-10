#pragma once

#include "Component/PrimitiveComponent.h"
#include "Render/Resource/Material.h"

class UDecalComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)
	UDecalComponent();

	UDecalComponent* Duplicate() override;
	UDecalComponent* DuplicateSubObjects() override { return this; }

	void SetMaterial(FMaterial* InMaterial) { Material = InMaterial; }
	FMaterial* GetMaterial() const { return Material; }

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_Decal; }

	FMatrix GetDecalMatrix() const;
	FColor GetDecalColor() const { return DecalColor; }

private:
	FMaterial* Material = nullptr;
	FVector DecalSize = FVector(5.0f, 5.0f, 5.0f);
	FColor DecalColor = FColor::White();
};
