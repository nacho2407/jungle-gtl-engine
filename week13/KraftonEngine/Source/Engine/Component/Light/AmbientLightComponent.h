#pragma once
#include "Component/Light/LightComponent.h"

class UAmbientLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UAmbientLightComponent, ULightComponent)
	UAmbientLightComponent();

	virtual ELightComponentType GetLightType() const override { return ELightComponentType::Ambient; }
	virtual void PushToScene() override;
	virtual void DestroyFromScene() override;
};