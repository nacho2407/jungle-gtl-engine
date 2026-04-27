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

    if (UDirectionalLightComponent* DirectionalLight = Cast<UDirectionalLightComponent>(Owner))
    {
        CascadeCount = std::clamp(DirectionalLight->GetCascadeCount(), 1, 4);
        DynamicShadowDistance = DirectionalLight->GetDynamicShadowDistance();
        CascadeDistribution = DirectionalLight->GetCascadeDistribution();
    }
}

void FLightProxy::UpdateTransform()
{
    if (!Owner)
        return;
    LightProxyInfo.Position  = Owner->GetWorldLocation();
    LightProxyInfo.Direction = Owner->GetForwardVector();
}

