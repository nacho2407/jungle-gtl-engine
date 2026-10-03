#pragma once

#include "DecalComponent.generated.h"

#include "Component/PrimitiveComponent.h"

class UMaterialInterface;

UCLASS()
class UDecalComponent : public UPrimitiveComponent
{
    GENERATED_BODY_UDecalComponent()
public:
	DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)

	UDecalComponent();

	void PostDuplicate(UObject* Original) override;

	virtual void Serialize(FArchive& Ar) override;

	void BeginPlay() override;

	virtual void SetMaterial(int32 SlotIndex, UMaterialInterface* InMaterial) override { if (SlotIndex == 0) Materials[0] = InMaterial; }
	virtual UMaterialInterface* GetMaterial(int32 SlotIndex) const override { return (SlotIndex == 0) ? Materials[0] : nullptr; }
	virtual int32 GetNumMaterials() const override { return 1; }

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
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
    UPROPERTY(EditAnywhere)
	TArray<UMaterialInterface*> Materials;
    UPROPERTY(EditAnywhere)
	FVector DecalSize = FVector(5.0f, 5.0f, 5.0f);
    UPROPERTY(EditAnywhere)
	FColor DecalColor = FColor::White();
	bool bDebugLine = true;

    UPROPERTY(EditAnywhere)
	float FadeStartDelay = 0.0f;
    UPROPERTY(EditAnywhere)
	float FadeDuration = 0.0f;
    UPROPERTY(EditAnywhere)
	float FadeInDuration = 0.0f;
    UPROPERTY(EditAnywhere)
	float FadeInStartDelay = 0.0f;
    UPROPERTY(EditAnywhere)
	bool bDestroyOwnerAfterFade = false;

	float LifeTime = 0.0f;
};
