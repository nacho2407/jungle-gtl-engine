#pragma once

#include "Render/Scene/Proxies/Light/PointLightSceneProxy.h"

class USpotLightComponent;

class FSpotLightSceneProxy : public FPointLightSceneProxy
{
public:
    FSpotLightSceneProxy(USpotLightComponent* InComponent);
    ~FSpotLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void UpdateTransform() override;
    void VisualizeLightsInEditor(FScene& Scene) const override;
};