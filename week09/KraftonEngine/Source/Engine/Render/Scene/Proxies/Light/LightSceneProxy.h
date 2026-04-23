#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resources/Buffers/LightBufferTypes.h"
#include "Render/Scene/Proxies/SceneProxy.h"

class ULightComponent;
class FScene;

class FLightSceneProxy : public FSceneProxy
{
public:
    FLightSceneProxy(ULightComponent* InComponent);
    virtual ~FLightSceneProxy() = default;

    virtual void UpdateLightConstants();
    virtual void UpdateTransform();

    virtual void VisualizeLightsInEditor(FScene& Scene) const {}

    ULightComponent* Owner = nullptr;

    FLightConstants LightConstants = {};

    bool bVisible      = true;
    bool bAffectsWorld = true;
};
