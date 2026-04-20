#pragma once

#include "Render/Scene/Proxies/Light/LightSceneProxy.h"

class UDirectionalLightComponent;

class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:
    FDirectionalLightSceneProxy(UDirectionalLightComponent* InComponent);
    ~FDirectionalLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void VisualizeLightsInEditor(FScene& Scene) const override;
};