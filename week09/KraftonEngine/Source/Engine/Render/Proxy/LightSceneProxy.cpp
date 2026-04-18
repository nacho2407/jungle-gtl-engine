#include "Render/Proxy/LightSceneProxy.h"
#include "Render/Proxy/FScene.h"
#include "Component/LightComponentBase.h"
#include "GameFramework/AActor.h"

// ============================================================
// FPrimitiveSceneProxy — 기본 구현
// ============================================================
FLightSceneProxy::FLightSceneProxy(ULightComponentBase* InComponent)
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
    LightConstants.Position = Owner->GetWorldLocation();
    LightConstants.Direction = FVector(0, 0, -1);
}