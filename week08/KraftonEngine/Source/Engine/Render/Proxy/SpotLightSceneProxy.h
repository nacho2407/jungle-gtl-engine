#pragma once

#include "PointLightSceneProxy.h"

class FSpotLightSceneProxy : public FPointLightSceneProxy
{
public:
    FSpotLightSceneProxy(ULightComponentBase* InComponent);
    ~FSpotLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void UpdateTransform() override;
};