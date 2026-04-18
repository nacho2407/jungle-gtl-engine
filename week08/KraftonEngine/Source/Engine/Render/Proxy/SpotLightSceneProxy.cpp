#include "SpotLightSceneProxy.h"
#include "Component/SpotLightComponent.h"

FSpotLightSceneProxy::FSpotLightSceneProxy(USpotLightComponent* InComponent)
    : FPointLightSceneProxy(InComponent)
{
}

void FSpotLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
        return;

    FPointLightSceneProxy::UpdateLightConstants();

    USpotLightComponent* SpotLight = static_cast<USpotLightComponent*>(Owner);
    LightConstants.InnerConeAngle = SpotLight->GetInnerConeAngle();
    LightConstants.OuterConeAngle = SpotLight->GetOuterConeAngle();
    LightConstants.LightType = static_cast<uint32>(ELightType::Spot);
}

void FSpotLightSceneProxy::UpdateTransform()
{
    if (!Owner)
        return;
    LightConstants.Position = Owner->GetWorldLocation();
    LightConstants.Direction = Owner->GetForwardVector();
}