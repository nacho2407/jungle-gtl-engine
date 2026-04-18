#pragma once

#include "LightSceneProxy.h"

class FPointLightSceneProxy : public FLightSceneProxy
{
public:
    FPointLightSceneProxy(ULightComponentBase* InComponent);
    ~FPointLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void UpdateTransform() override;
};