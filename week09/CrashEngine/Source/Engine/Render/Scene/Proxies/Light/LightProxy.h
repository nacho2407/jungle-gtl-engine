// Declares the shared base proxy used by all renderable light types.
#pragma once

#include "Core/CoreTypes.h"
#include "Render/Scene/Proxies/Light/LightProxyInfo.h"
#include "Render/Scene/Proxies/SceneProxy.h"
#include "Engine/Math/Matrix.h"
#include "Render/Visibility/Frustum/ConvexVolume.h"

class ULightComponent;
class FPrimitiveProxy;
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

    // --- Shadow Related ---
    TArray<FPrimitiveProxy*> VisibleShadowCasters;
    FConvexVolume            ShadowViewFrustum;
    FMatrix                  LightViewProj;
    FMatrix                  ShadowViewProjMatrices[6]; // For Point Light (Cube faces)
    int32                    ShadowMapIndex = -1;
    bool                     bCastShadow = false;
};

using FLightSceneProxy = FLightProxy;

