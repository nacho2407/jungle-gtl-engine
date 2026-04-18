#include "PointLightSceneProxy.h"
#include "Component/PointLightComponent.h"

FPointLightSceneProxy::FPointLightSceneProxy(UPointLightComponent* InComponent)
    : FLightSceneProxy(InComponent)
{
}

void FPointLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
        return;

    FLightSceneProxy::UpdateLightConstants();

    UPointLightComponent* PointLight = static_cast<UPointLightComponent*>(Owner);
    LightConstants.AttenuationRadius = PointLight->GetAttenuationRadius();
    LightConstants.LightType = static_cast<uint32>(ELightType::Point);
}

void FPointLightSceneProxy::UpdateTransform()
{
    if (!Owner)
        return;
    LightConstants.Position = Owner->GetWorldLocation();
}
