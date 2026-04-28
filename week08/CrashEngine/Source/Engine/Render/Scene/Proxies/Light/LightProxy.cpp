// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Component/LightComponent.h"
#include "Component/DirectionalLightComponent.h"

#include <algorithm>

FLightProxy::FLightProxy(ULightComponent* InComponent)
    : Owner(InComponent)
{
}

void FLightProxy::UpdateLightConstants()
{
    if (!Owner)
        return;
    bAffectsWorld         = Owner->AffectsWorld();
    bCastShadow           = Owner->DoesCastShadows();
    ShadowResolution      = Owner->GetShadowResolution();
    ShadowBias            = Owner->GetShadowBias();
    ShadowSlopeBias       = Owner->GetShadowSlopeBias();
    ShadowNormalBias      = Owner->GetShadowNormalBias();
    
    LightProxyInfo.Position    = Owner->GetWorldLocation();
    LightProxyInfo.Direction   = Owner->GetForwardVector();
    LightProxyInfo.Intensity   = Owner->GetIntensity();
    LightProxyInfo.LightColor  = Owner->GetLightColor();
    LightProxyInfo.ShadowBias  = ShadowBias;
    LightProxyInfo.ShadowSlopeBias = ShadowSlopeBias;
    LightProxyInfo.ShadowNormalBias = ShadowNormalBias;
}

void FLightProxy::UpdateTransform()
{
    if (!Owner)
        return;
    LightProxyInfo.Position  = Owner->GetWorldLocation();
    LightProxyInfo.Direction = Owner->GetForwardVector();
}

void FLightProxy::ClearShadowData()
{
    if (FCascadeShadowMapData* CascadeShadowMapData = GetCascadeShadowMapData())
    {
        CascadeShadowMapData->Reset();
    }

    if (FShadowMapData* SpotShadowMapData = GetSpotShadowMapData())
    {
        SpotShadowMapData->Reset();
    }

    if (FCubeShadowMapData* CubeShadowMapData = GetCubeShadowMapData())
    {
        CubeShadowMapData->Reset();
    }
}

void FLightProxy::ApplyShadowRecord(const FLightShadowRecord& Record)
{
    if (FCascadeShadowMapData* CascadeShadowMapData = GetCascadeShadowMapData())
    {
        *CascadeShadowMapData = Record.CascadeShadowMapData;
    }

    if (FShadowMapData* SpotShadowMapData = GetSpotShadowMapData())
    {
        *SpotShadowMapData = Record.SpotShadowMapData;
    }

    if (FCubeShadowMapData* CubeShadowMapData = GetCubeShadowMapData())
    {
        *CubeShadowMapData = Record.CubeShadowMapData;
    }
}

