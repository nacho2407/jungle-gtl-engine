#pragma once
#include "Component/PrimitiveComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

class UDecalComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)

	UDecalComponent() = default;
	~UDecalComponent() override = default;
	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void UpdateLocalExtents();

	void SetTexture(const FName& InTextureName);
	void SetDecalSize(const FVector& InSize);

private:

	FTextureResource* CachedTexture = nullptr;
	FName TextureName;
	FVector DecalSize = { 100.0f, 100.0f, 100.0f };
};

