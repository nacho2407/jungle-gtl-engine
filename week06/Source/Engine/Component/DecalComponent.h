#pragma once

#include "PrimitiveComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

struct FFadeSetting
{
	float FadeAlpha = 1.0f;
	float FadeInTime = 1.0f;
	float FadeOutTime = 1.0f;
	float TotalLifetime = 5.0f;
	float ElapsedTime = 0.0f;
	bool bFadeOnceAndDestroy = false;

	void Reset()
	{
		ElapsedTime = 0.0f;
		FadeAlpha = 1.0f;
	}
};

class UDecalComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)

	UDecalComponent();
	~UDecalComponent() override = default;

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	bool SupportsOutline() const override { return false; }
	bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;

	void SetTexture(const FName& InTextureName);
	const FName& GetTextureName() const { return TextureName; }
	const FTextureResource* GetTexture() const { return CachedTexture; }

	void SetFadeAlpha(float InFadeAlpha);
	void SetFadeSetting(FFadeSetting InFadeSetting) { FadeSetting = InFadeSetting; }
	float GetFadeAlpha() const { return FadeSetting.FadeAlpha; }

	void SetDecalSize(const FVector& InDecalSize);
	const FVector& GetDecalSize() const { return DecalSize; }
	FVector GetHalfExtents() const { return DecalSize * 0.5f; }
	FVector4 GetDecalColor() const { return DecalColor; }

	void ResetFade();

	FMatrix& GetDecalToWorldMatrix() const;
	FMatrix& GetWorldToDecalMatrix() const;

	void GetWorldCorners(FVector(&OutCorners)[8]) const;

protected:
	void OnTransformDirty() override;

private:
	void MarkDecalDirty();
	void UpdateDecalMatrices() const;

	FName TextureName = FName::None;
	FTextureResource* CachedTexture = nullptr;
	FVector4 DecalColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	FVector DecalSize = FVector(1.0f, 1.0f, 1.0f);
	FFadeSetting FadeSetting = {};

	mutable FMatrix DecalToWorld = FMatrix::Identity;
	mutable FMatrix WorldToDecal = FMatrix::Identity;
	mutable bool bDecalMatrixDirty = true;
};
