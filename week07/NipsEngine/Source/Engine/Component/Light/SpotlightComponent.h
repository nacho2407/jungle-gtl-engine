#pragma once

#include "PointLightComponent.h"

class USpotLightComponent : public UPointLightComponent
{
public:
	DECLARE_CLASS(USpotLightComponent, UPointLightComponent)

	USpotLightComponent() = default;
	virtual ~USpotLightComponent() override = default;

	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	virtual void Serialize(FArchive& Ar) override;

	FVector GetLightDirection() const { return GetForwardVector(); }

	float GetInnerConeAngle() const { return InnerConeAngle; }
	void SetInnerConeAngle(float InAngle);
	float GetOuterConeAngle() const { return OuterConeAngle; }
	void SetOuterConeAngle(float InAngle);

private:
	float InnerConeAngle = 15.0f;
	float OuterConeAngle = 30.0f;
};
