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
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void SetTexture(const FName& InTextureName);
	void SetDecalSize(const FVector& InSize);
	const FTextureResource* GetTexture() const { return CachedTexture; }
	const FVector& GetDecalSize() const { return DecalSize; }

	FMatrix& GetDecalToWorldMatrix() const;
	FMatrix& GetWorldToDecalMatrix() const;

private:
	void UpdateDecalMatrices() const;
private:
	FTextureResource* CachedTexture = nullptr;
	FName TextureName;

	mutable FMatrix DecalToWorld = FMatrix::Identity;
	mutable FMatrix WorldToDecal = FMatrix::Identity;
	mutable bool bDecalMatrixDirty = true;

	FVector DecalSize = { 100.0f, 100.0f, 100.0f };
};

