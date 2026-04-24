// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Scene/Proxies/Light/LightSceneProxy.h"
#include "Component/LightComponent.h"

FLightSceneProxy::FLightSceneProxy(ULightComponent* InComponent)
    : Owner(InComponent)
{
}

void FLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
        return;
    bAffectsWorld             = Owner->AffectsWorld();
    LightConstants.Position   = Owner->GetWorldLocation();
    LightConstants.Direction  = Owner->GetForwardVector();
    LightConstants.Intensity  = Owner->GetIntensity();
    LightConstants.LightColor = Owner->GetLightColor();
}

void FLightSceneProxy::UpdateTransform()
{
    if (!Owner)
        return;
    LightConstants.Position  = Owner->GetWorldLocation();
    LightConstants.Direction = Owner->GetForwardVector();
}
