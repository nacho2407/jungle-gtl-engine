// Declares the shared base proxy used by all renderable light types.
#pragma once

#include "Core/CoreTypes.h"
#include "Render/Scene/Proxies/Light/LightProxyInfo.h"
#include "Render/Scene/Proxies/SceneProxy.h"

class ULightComponent;
class FScene;

// FLightProxy converts a light component into renderer submission data.
class FLightProxy : public FSceneProxy
{
public:
    FLightProxy(ULightComponent* InComponent);
    virtual ~FLightProxy() = default;

    virtual void UpdateLightConstants();
    virtual void UpdateTransform();

    virtual void VisualizeLightsInEditor(FScene& Scene) const {}

    ULightComponent* Owner = nullptr;

    FLightProxyInfo LightProxyInfo = {};

    bool bVisible      = true;
    bool bAffectsWorld = true;
};

