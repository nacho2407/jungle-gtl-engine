#pragma once
#include "PrimitiveComponent.h"
#include "Core/ResourceTypes.h"

class UFireballComponent : public UPrimitiveComponent {
public:
	DECLARE_CLASS(UFireballComponent, UPrimitiveComponent)
	UFireballComponent() = default;
	~UFireballComponent() override = default;

	//virtual UFireballComponent* Duplicate() override;
	//virtual UFireballComponent* DuplicateSubObjects() override { return this; }

	void PostDuplicate(UObject* original) override; 

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
    static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Fireball;

    bool SupportsOutline() const override { return true; }

	void UpdateWorldAABB() const override;
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;


	// Accessors (Probably redundant, remove later)
	float GetIntensity()	 const { return Intensity; }
	float GetRadius()		 const { return Radius; }
	float GetRadiusFallOff() const { return RadiusFallOff; }
	FColor& GetLinearColor() { return Color; }

	void SetIntensity(float InIntensity) { Intensity = InIntensity; }
	void SetRadius(float InRadius) { if (InRadius) Radius = InRadius; }
	void SetRadiusFallOff(float InFallOff) { if (InFallOff) RadiusFallOff = InFallOff; }

private:
	float  Intensity		= 1.f;
	float  Radius			= 15.f;
	float  RadiusFallOff	= 1.f;
	FColor Color			= FColor(1.0f, 0.8f, 0.04f, 1.f);
};