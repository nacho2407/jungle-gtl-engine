#pragma once
#include "LightComponent.h"

UCLASS(SpawnableComponent, DisplayName = "AmbientLight Component", Category = "Light")
class UAmbientLightComponent : public ULightComponent {
public:
	GENERATED_BODY(UAmbientLightComponent, ULightComponent)
	UAmbientLightComponent() = default;
};
