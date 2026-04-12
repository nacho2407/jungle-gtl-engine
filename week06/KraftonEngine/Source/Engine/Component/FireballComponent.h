#pragma once
#include "Component/PrimitiveComponent.h"
class UFireballComponent : public UPrimitiveComponent
{
	public:
	DECLARE_CLASS(UFireballComponent, UPrimitiveComponent)
	UFireballComponent() = default;
	~UFireballComponent() override = default;
	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	float GetIntensity() const { return Intensity; }
	float GetRadius() const { return Radius; }
	float GetRadiusFalloff() const { return RadiusFalloff; }
	const FVector& GetColor() const { return Color; }


private:
	float Intensity = 1.0f;
	float Radius = 100.0f;
	float RadiusFalloff = 1.0f;
	FVector Color;
};

