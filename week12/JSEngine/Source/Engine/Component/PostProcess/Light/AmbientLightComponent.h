#pragma once
#include "LightComponent.h"

UCLASS()
class UAmbientLightComponent : public ULightComponent {
public:
	DECLARE_CLASS(UAmbientLightComponent, ULightComponent)
	UAmbientLightComponent() = default;
};
