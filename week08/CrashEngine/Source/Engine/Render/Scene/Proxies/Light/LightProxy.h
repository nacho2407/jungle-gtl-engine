// Declares the shared base proxy used by all renderable light types.
#pragma once

#include "Core/CoreTypes.h"
#include "Engine/Math/Matrix.h"
#include "Render/Scene/Proxies/Light/LightProxyInfo.h"
#include "Render/Scene/Proxies/SceneProxy.h"
#include "Render/Submission/Atlas/ShadowAtlasTypes.h"
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

    virtual FCascadeShadowMapData*       GetCascadeShadowMapData() { return nullptr; }
    virtual const FCascadeShadowMapData* GetCascadeShadowMapData() const { return nullptr; }
    virtual FShadowMapData*              GetSpotShadowMapData() { return nullptr; }
    virtual const FShadowMapData*        GetSpotShadowMapData() const { return nullptr; }
    virtual FShadowViewData*             GetSpotShadowView() { return nullptr; }
    virtual const FShadowViewData*       GetSpotShadowView() const { return nullptr; }
    virtual FCubeShadowMapData*          GetCubeShadowMapData() { return nullptr; }
    virtual const FCubeShadowMapData*    GetCubeShadowMapData() const { return nullptr; }
    virtual FMatrix*                     GetPointShadowViewProjMatrices() { return nullptr; }
    virtual const FMatrix*               GetPointShadowViewProjMatrices() const { return nullptr; }
    virtual int32                        GetCascadeCountSetting() const { return 1; }
    virtual float                        GetDynamicShadowDistanceSetting() const { return 2000.0f; }
    virtual float                        GetCascadeDistributionSetting() const { return 1.0f; }

    void ClearShadowData();
    void ApplyShadowRecord(const FLightShadowRecord& Record);

    ULightComponent* Owner = nullptr;

    FLightProxyInfo LightProxyInfo = {};

    bool bVisible      = true;
    bool bAffectsWorld = true;

    TArray<FPrimitiveProxy*> VisibleShadowCasters;
    FConvexVolume            ShadowViewFrustum;
    FMatrix                  LightViewProj;
    bool                     bCastShadow      = false;
    uint32                   ShadowResolution = 1024;

    float ShadowBias       = 0.0f;
    float ShadowSlopeBias  = 0.0f;
    float ShadowNormalBias = 0.0f;
};
