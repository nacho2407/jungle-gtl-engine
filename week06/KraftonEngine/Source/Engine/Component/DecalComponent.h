#pragma once

#include "PrimitiveComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

class UDecalComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)

	UDecalComponent();

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	bool SupportsOutline() const override { return false; }
	bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;

	void SetTexture(const FName& InTextureName);
	const FName& GetTextureName() const { return TextureName; }
	const FTextureResource* GetTexture() const { return CachedTexture; }

	void SetFadeAlpha(float InFadeAlpha);
	float GetFadeAlpha() const { return FadeAlpha; }

	void SetDecalSize(const FVector& InDecalSize);
	const FVector& GetDecalSize() const { return DecalSize; }
	FVector GetHalfExtents() const { return DecalSize * 0.5f; }

	void GetWorldCorners(FVector(&OutCorners)[8]) const;

private:
	FName TextureName = FName::None;
	FTextureResource* CachedTexture = nullptr;
	float FadeAlpha = 1.0f;
	FVector DecalSize = FVector(1.0f, 1.0f, 1.0f);
};
