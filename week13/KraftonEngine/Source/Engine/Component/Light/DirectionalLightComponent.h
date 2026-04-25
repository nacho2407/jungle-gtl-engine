#pragma once
#include "Component/Light/LightComponentBase.h"
#include "Component/Light/LightComponent.h"

class UDirectionalLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)

	virtual ELightComponentType GetLightType() const override { return ELightComponentType::Directional; }
	void ContributeSelectedVisuals(FScene& Scene) const;
	virtual void PushToScene() override;
	virtual void DestroyFromScene() override;
	virtual bool GetLightViewProj(FLightViewProjResult& OutResult, const UCameraComponent* Camera = nullptr, int32 FaceIndex = 0) const override;
};
