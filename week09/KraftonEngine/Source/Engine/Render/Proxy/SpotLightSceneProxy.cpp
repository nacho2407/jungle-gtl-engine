#include "SpotLightSceneProxy.h"
#include "Component/LightComponentBase.h"

void FSpotLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
        return;
	
	FLightSceneProxy::UpdateLightConstants();

	LightConstants.InnerConeAngle;
	LightConstants.OuterConeAngle;
}

void FSpotLightSceneProxy::UpdateTransform()
{
	if (!Owner)
		return;
    LightConstants.Position = Owner->GetWorldLocation();
    LightConstants.Direction = FVector(0, 0, -1);
}