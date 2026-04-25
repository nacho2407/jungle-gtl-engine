#pragma once
#include "Component/Light/LightComponent.h"

class UPointLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UPointLightComponent, ULightComponent)
	virtual ELightComponentType GetLightType() const override { return ELightComponentType::Point; }
	virtual void ContributeSelectedVisuals(FScene& Scene) const override;
	virtual void PushToScene() override;
	virtual void DestroyFromScene() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	virtual bool GetLightViewProj(FLightViewProjResult& OutResult, const UCameraComponent* Camera = nullptr, int32 FaceIndex = 0) const override;

	float GetAttenuationRadius() const { return AttenuationRadius; }

protected:
	float AttenuationRadius = 1.f;
	float LightFalloffExponent = 1.f;
};
