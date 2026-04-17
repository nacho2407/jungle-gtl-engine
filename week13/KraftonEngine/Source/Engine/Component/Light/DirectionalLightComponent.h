#pragma once
#include "Component/Light/LightComponent.h"

class UDirectionalLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)

	void ContributeSelectedVisuals(FScene& Scene) const;
	virtual void PushToScene() override;
	virtual void DestroyFromScene() override;
};
