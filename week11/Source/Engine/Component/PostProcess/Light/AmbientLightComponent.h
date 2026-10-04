#pragma once
#include "LightComponent.h"

#include "AmbientLightComponent.generated.h"

UCLASS()
class UAmbientLightComponent : public ULightComponent {
    GENERATED_BODY_UAmbientLightComponent()
public:
	DECLARE_CLASS(UAmbientLightComponent, ULightComponent)
	UAmbientLightComponent() = default;
};
