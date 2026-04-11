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
	
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction);

	void SetTexture(const FName& InTextureName);
	void SetDecalSize(const FVector& InSize);
	const FTextureResource* GetTexture() const { return CachedTexture; }
	const FVector& GetDecalSize() const { return DecalSize; }
	float GetFadeAlpha() const { return FadeAlpha; }
	FVector4 GetDecalColor() const { return DecalColor; }

	void ResetFade();

	FMatrix& GetDecalToWorldMatrix() const;
	FMatrix& GetWorldToDecalMatrix() const;


protected:
	void OnTransformDirty() override;

private:
	void MarkDecalDirty();
	void UpdateDecalMatrices() const;
private:
	FTextureResource* CachedTexture = nullptr;
	FName TextureName;

	mutable FMatrix DecalToWorld = FMatrix::Identity;
	mutable FMatrix WorldToDecal = FMatrix::Identity;
	mutable bool bDecalMatrixDirty = true;

	FVector DecalSize = { 1.0f, 1.0f, 1.0f };
	FVector4 DecalColor = { 1.0f, 1.0f, 1.0f , 1.0f};

	float FadeAlpha = 1.0f;
	float FadeInTime = 1.0f;
	float FadeOutTime = 1.0f;
	float TotalLifetime = 5.0f;
	float ElapsedTime = 0.0f;
};

