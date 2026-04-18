#pragma once

#include "LightSceneProxy.h"

class UDirectionalLightComponent;

class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:
    FDirectionalLightSceneProxy(UDirectionalLightComponent* InComponent);
    ~FDirectionalLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void UpdateTransform() override;
};