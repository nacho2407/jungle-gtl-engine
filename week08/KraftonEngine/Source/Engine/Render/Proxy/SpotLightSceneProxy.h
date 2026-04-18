#pragma once

#include "PointLightSceneProxy.h"

class USpotLightComponent;

class FSpotLightSceneProxy : public FPointLightSceneProxy
{
public:
    FSpotLightSceneProxy(USpotLightComponent* InComponent);
    ~FSpotLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void UpdateTransform() override;
};