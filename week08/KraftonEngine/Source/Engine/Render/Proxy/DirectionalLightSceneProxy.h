#pragma once

#include "LightSceneProxy.h"

class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:
    FDirectionalLightSceneProxy(ULightComponentBase* InComponent);
    ~FDirectionalLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void UpdateTransform() override;
}