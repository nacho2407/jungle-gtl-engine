#pragma once

#include "Component/PrimitiveComponent.h"
#include "Render/Resource/Material.h"

UCLASS()
class UDecalComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY(UDecalComponent, UPrimitiveComponent)

	UDecalComponent();

	virtual void Serialize(FArchive& Ar) override;

	void BeginPlay() override;

	virtual void SetMaterial(int32 SlotIndex, UMaterialInterface* InMaterial) override { if (SlotIndex == 0) Materials[0] = InMaterial; }
	virtual UMaterialInterface* GetMaterial(int32 SlotIndex) const override { return (SlotIndex == 0) ? Materials[0] : nullptr; }
	virtual int32 GetNumMaterials() const override { return 1; }

	void PostEditProperty(const char* PropertyName) override;

	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_Decal; }

	FMatrix GetDecalMatrix() const;
	FColor GetDecalColor() const { return DecalColor; }

	void SetSize(const FVector& InSize) { DecalSize = InSize; }

	void SetFadeIn(float InStartDelay, float InDuration);
	void SetFadeOut(float InStartDelay, float InDuration, bool bInDestroyOwnerAfterFade = false);

	bool SupportsOutline() const override { return true; }

protected:
	void TickComponent(float DeltaTime) override;

private:
	void TickFadeIn();
	void TickFadeOut();

private:
	UPROPERTY(DisplayName = "Materials", ReferenceType = Asset)
	TArray<UMaterialInterface*> Materials;

	UPROPERTY(DisplayName = "Size")
	FVector DecalSize = FVector(5.0f, 5.0f, 5.0f);

	UPROPERTY(DisplayName = "Color")
	FColor DecalColor = FColor::White();

	bool bDebugLine = true;

	UPROPERTY(DisplayName = "Fade Start Delay")
	float FadeStartDelay = 0.0f;

	UPROPERTY(DisplayName = "Fade Duration")
	float FadeDuration = 0.0f;

	UPROPERTY(DisplayName = "Fade In Duration")
	float FadeInDuration = 0.0f;

	UPROPERTY(DisplayName = "Fade In Start Delay")
	float FadeInStartDelay = 0.0f;

	UPROPERTY(DisplayName = "Destroy Owner After Fade")
	bool bDestroyOwnerAfterFade = false;

	float LifeTime = 0.0f;
};
