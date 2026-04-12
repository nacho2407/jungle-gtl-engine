#pragma once

#include "Component/PrimitiveComponent.h"

class UMaterialInterface;

class UDecalComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)
	UDecalComponent();

	void PostDuplicate(UObject* Original) override;

	void BeginPlay() override;

	void SetMaterial(UMaterialInterface* InMaterial) { OverrideMaterial = InMaterial; }
	UMaterialInterface* GetMaterial() const { return OverrideMaterial; }

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_Decal; }

	FMatrix GetDecalMatrix() const;
	FColor GetDecalColor() const { return DecalColor; }

	void SetFadeIn(float InStartDelay, float InDuration);
	void SetFadeOut(float InStartDelay, float InDuration, bool bInDestroyOwnerAfterFade = false);

protected:
	void TickComponent(float DeltaTime) override;

private:
	void TickFadeIn();
	void TickFadeOut();

private:
	UMaterialInterface* OverrideMaterial = nullptr;
	FVector DecalSize = FVector(5.0f, 5.0f, 5.0f);
	FColor DecalColor = FColor::White();

	float FadeStartDelay = 0.0f;
	float FadeDuration = 0.0f;
	float FadeInDuration = 0.0f;
	float FadeInStartDelay = 0.0f;
	bool bDestroyOwnerAfterFade = false;

	float LifeTime = 0.0f;
};
