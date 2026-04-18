#include "Render/Proxy/LightSceneProxy.h"
#include "Component/LightComponent.h"

FLightSceneProxy::FLightSceneProxy(ULightComponent* InComponent)
	: Owner(InComponent)
{
}

void FLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
        return;
    LightConstants.Position = Owner->GetWorldLocation();
    LightConstants.Intensity = Owner->GetIntensity();
    LightConstants.LightColor = Owner->GetLightColor();
}

void FLightSceneProxy::UpdateTransform()
{
    if (!Owner)
        return;
    LightConstants.Position  = Owner->GetWorldLocation();
    LightConstants.Direction = Owner->GetForwardVector();
}