#pragma once

#include "LightSceneProxy.h"

class UPointLightComponent;

class FPointLightSceneProxy : public FLightSceneProxy
{
public:
    FPointLightSceneProxy(UPointLightComponent* InComponent);
    ~FPointLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void UpdateTransform() override;
};