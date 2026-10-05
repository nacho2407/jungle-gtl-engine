#pragma once
#include "PrimitiveComponent.h"
#include "Core/ResourceTypes.h"

UCLASS()
class UFireballComponent : public UPrimitiveComponent {
public:
	GENERATED_BODY(UFireballComponent, UPrimitiveComponent)

	UFireballComponent();
	~UFireballComponent() override = default;

	//virtual UFireballComponent* Duplicate() override;
	//virtual UFireballComponent* DuplicateSubObjects() override { return this; }

	EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_Fireball; }

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
	UPROPERTY(DisplayName = "Intensity", LuaReadWrite, LuaName = Intensity)
	float  Intensity		= 1.f;

	UPROPERTY(DisplayName = "Radius", LuaReadWrite, LuaName = Radius)
	float  Radius			= 15.f;

	UPROPERTY(DisplayName = "Radius Falloff", LuaReadWrite, LuaName = RadiusFallOff)
	float  RadiusFallOff	= 1.f;

	UPROPERTY(DisplayName = "Color")
	FColor Color			= FColor(1.0f, 0.8f, 0.04f, 1.f);
};
