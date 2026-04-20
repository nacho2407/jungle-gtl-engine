#pragma once

#include "Render/Scene/Proxies/Light/LightSceneProxy.h"

class UPointLightComponent;

class FPointLightSceneProxy : public FLightSceneProxy
{
public:
    FPointLightSceneProxy(UPointLightComponent* InComponent);
    ~FPointLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void UpdateTransform() override;
    void VisualizeLightsInEditor(FScene& Scene) const override;
};