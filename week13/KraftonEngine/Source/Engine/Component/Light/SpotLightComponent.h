#pragma once
#include "Component/Light/PointLightComponent.h"

class USpotLightComponent : public UPointLightComponent
{
public:
	DECLARE_CLASS(USpotLightComponent, UPointLightComponent)
	static void RegisterProperties(UClass* Class);
	virtual ELightComponentType GetLightType() const override { return ELightComponentType::Spot; }
	virtual void ContributeSelectedVisuals(FScene& Scene) const override;
	virtual void PushToScene() override;
	virtual void DestroyFromScene() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	virtual bool GetLightViewProj(FLightViewProjResult& OutResult, const FMinimalViewInfo* POV = nullptr, int32 FaceIndex = 0) const override;

	float GetOuterConeAngle() const { return OuterConeAngle; }

protected:
	UPROPERTY(Edit, Save, Category="Lighting", DisplayName="InnerConeAngle", Min=0.0f, Max=89.0f, Speed=0.1f)
	float InnerConeAngle = 20.0f;	// Inner Cone Angle in degrees
	UPROPERTY(Edit, Save, Category="Lighting", DisplayName="OuterConeAngle", Min=0.0f, Max=89.0f, Speed=0.1f)
	float OuterConeAngle = 40.0f;	// Outer Cone Angle in degrees
};
