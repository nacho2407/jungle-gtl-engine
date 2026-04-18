#include "DirectionalLightSceneProxy.h"
#include "Component/DirectionalLightComponent.h"

FDirectionalLightSceneProxy::FDirectionalLightSceneProxy(UDirectionalLightComponent* InComponent)
    : FLightSceneProxy(InComponent)
{
}

void FDirectionalLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
        return;

    FLightSceneProxy::UpdateLightConstants();

    UDirectionalLightComponent* DirectionalLight = static_cast<UDirectionalLightComponent*>(Owner);
    LightConstants.LightType = static_cast<uint32>(ELightType::Directional);
}

void FDirectionalLightSceneProxy::UpdateTransform()
{
    if (!Owner)
        return;
    LightConstants.Position = Owner->GetWorldLocation();
    LightConstants.Direction = Owner->GetForwardVector();
}