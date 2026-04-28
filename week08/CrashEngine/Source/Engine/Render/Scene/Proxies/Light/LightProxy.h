// Declares the shared base proxy used by all renderable light types.
#pragma once

#include "Core/CoreTypes.h"
#include "Engine/Math/Matrix.h"
#include "Render/Scene/Proxies/Light/LightProxyInfo.h"
#include "Render/Scene/Proxies/SceneProxy.h"
#include "Render/Submission/Atlas/ShadowAtlasSystem.h"
#include "Render/Visibility/Frustum/ConvexVolume.h"

class ULightComponent;
class FPrimitiveProxy;
class FScene;

class FLightProxy : public FSceneProxy
{
public:
    FLightProxy(ULightComponent* InComponent);
    virtual ~FLightProxy() = default;

    virtual void UpdateLightConstants();
    virtual void UpdateTransform();

    virtual void VisualizeLightsInEditor(FScene& Scene) const {}

    void ClearShadowData();
    void ApplyShadowRecord(const FLightShadowRecord& Record);

    ULightComponent* Owner = nullptr;

    FLightProxyInfo LightProxyInfo = {};

    bool bVisible      = true;
    bool bAffectsWorld = true;

    TArray<FPrimitiveProxy*> VisibleShadowCasters;
    FConvexVolume            ShadowViewFrustum;
    FMatrix                  LightViewProj;
    FShadowViewData          LightShadowView;
    FMatrix                  ShadowViewProjMatrices[ShadowAtlas::MaxPointFaces];
    FShadowViewData          ShadowFaceViews[ShadowAtlas::MaxPointFaces];
    bool                     bCastShadow = false;
    uint32                   ShadowResolution = 1024;
    FCascadeShadowMapData    CascadeShadowMapData = {};
    FShadowMapData           SpotShadowMapData = {};
    FCubeShadowMapData       CubeShadowMapData = {};

    float ShadowBias = 0.005f;
    float ShadowSlopeBias = 0.5f;
    float ShadowNormalBias = 1.0f;
    int32 CascadeCount = 1;
    float DynamicShadowDistance = 2000.0f;
    float CascadeDistribution = 1.0f;
};
