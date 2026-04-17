#pragma once
#include "Component/Light/LightComponent.h"

class UAmbientLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UAmbientLightComponent, ULightComponent)
	UAmbientLightComponent();

	virtual void PushToScene() override;
	virtual void DestroyFromScene() override;
};