#pragma once
#include "LightComponent.h"

UCLASS()
class UAmbientLightComponent : public ULightComponent {
public:
	GENERATED_BODY(UAmbientLightComponent, ULightComponent)
	UAmbientLightComponent() = default;
};
