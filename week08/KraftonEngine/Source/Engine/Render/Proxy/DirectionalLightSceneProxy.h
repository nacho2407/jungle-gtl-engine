#pragma once

#include "LightSceneProxy.h"

class UAmbientLightComponent;

class FAmbientLightSceneProxy : public FLightSceneProxy
{
public:
    FAmbientLightSceneProxy(UAmbientLightComponent* InComponent);
    ~FAmbientLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void UpdateTransform() override;
};