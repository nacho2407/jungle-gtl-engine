// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Component/LightComponent.h"

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
    LightProxyInfo.Position    = Owner->GetWorldLocation();
    LightProxyInfo.Direction   = Owner->GetForwardVector();
    LightProxyInfo.Intensity   = Owner->GetIntensity();
    LightProxyInfo.LightColor  = Owner->GetLightColor();
}

void FLightProxy::UpdateTransform()
{
    if (!Owner)
        return;
    LightProxyInfo.Position  = Owner->GetWorldLocation();
    LightProxyInfo.Direction = Owner->GetForwardVector();
}

