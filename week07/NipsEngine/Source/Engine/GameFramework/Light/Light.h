#pragma once

#include "GameFramework/AActor.h"
#include "Component/Light/LightComponent.h"
#include "Object/Object.h"

class ALight: public AActor
{
public:
    DECLARE_CLASS(ALight, AActor)

    ALight() = default;

    virtual ULightComponent* GetLightComponent() const { return Cast<ULightComponent>(GetRootComponent()); }
};
